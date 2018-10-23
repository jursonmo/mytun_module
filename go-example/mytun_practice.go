package main

import (
	"encoding/binary"
	"fmt"
	"log"
	"mylog"
	"packet"
	"syscall"
	"unsafe"

	"github.com/jursonmo/go-tuntap/tuntap"
)

type ringbuffer_header struct {
	r, w             uint16
	size             uint32
	count, node_size uint16
}
type ringbuffer struct {
	hdr    ringbuffer_header
	magic  uint32
	buffer []byte
	mapBuf []byte
}

func (rb *ringbuffer) show() {
	fmt.Printf("rb: r=%d, w=%d,size=%d, count=%d,node_size=%d, magic=%d\n", rb.hdr.r, rb.hdr.w, rb.hdr.size, rb.hdr.count, rb.hdr.node_size, rb.magic)
}
func (rb *ringbuffer) encode(mapbuffer []byte) {

	mapbuf := (uintptr(unsafe.Pointer(&mapbuffer[0])))
	rb.hdr.r = *(*uint16)(unsafe.Pointer(mapbuf + 0))
	rb.hdr.w = *(*uint16)(unsafe.Pointer(mapbuf + 2))
	rb.hdr.size = *(*uint32)(unsafe.Pointer(mapbuf + 4))
	rb.hdr.count = *(*uint16)(unsafe.Pointer(mapbuf + 8))
	rb.hdr.node_size = *(*uint16)(unsafe.Pointer(mapbuf + 10))
	rb.magic = *(*uint32)(unsafe.Pointer(mapbuf + 12))
	rb.buffer = *(*[]byte)(unsafe.Pointer(mapbuf + 16))

	// pbytes := (*reflect.SliceHeader)(unsafe.Pointer(&rb.buffer))
	// pbytes.Data = mapbuf + 16
	// pbytes.Cap = len(mapbuffer) - 16
	// pbytes.Len = len(mapbuffer) - 16
	rb.buffer = mapbuffer[16:]
	rb.mapBuf = mapbuffer

	//test
	node_size := binary.LittleEndian.Uint16(mapbuffer[10:])
	fmt.Println("===========", rb.hdr.node_size, node_size)
	if rb.hdr.node_size != node_size {
		panic("rb.hdr.node_size != node_size")
	}
}

func encodeRB(mapbuffer []byte) *ringbuffer {
	rb := (*ringbuffer)(unsafe.Pointer(&mapbuffer[0]))
	rb.buffer = mapbuffer[16:]
	rb.mapBuf = mapbuffer
	return rb
}

func (rb *ringbuffer) test() {
	rb1 := (*ringbuffer)(unsafe.Pointer(&rb.mapBuf[0]))
	fmt.Println("========== test============")
	rb1.show()
}
func (rb *ringbuffer) encodeRW() {
	mapbuf := (uintptr(unsafe.Pointer(&rb.mapBuf[0])))
	rb.hdr.r = *(*uint16)(unsafe.Pointer(mapbuf + 0))
	rb.hdr.w = *(*uint16)(unsafe.Pointer(mapbuf + 2))
}

func (rb *ringbuffer) GetPktData() []byte {
	rb.encodeRW()
	if rb.hdr.r == rb.hdr.w {
		return nil
	}
	offset := int(rb.hdr.r) * int(rb.hdr.node_size)
	return rb.buffer[offset : offset+int(rb.hdr.node_size)]
}
func (rb *ringbuffer) ReleasePktData() {
	rb.hdr.r = (rb.hdr.r + 1) % rb.hdr.count
	binary.LittleEndian.PutUint16(rb.mapBuf, rb.hdr.r)
}

//结构体字段记录的是内存地址
// type ringbuffer_headerP struct {
// 	r, w       uintptr
// 	size       uintptr
// 	count, node_size uintptr
// }

// type ringbufferP struct {
// 	hdr   ringbuffer_headerP
// 	magic uintptr
// }

// func (rb *ringbufferP) encodeP(mapbuf uintptr) {
// 	rb.hdr.r = mapbuf + 0
// 	rb.hdr.w = mapbuf + 2
// 	rb.hdr.size = mapbuf + 4
// 	rb.hdr.count = mapbuf + 8
// 	rb.hdr.node_size = mapbuf + 10
// 	rb.magic = mapbuf + 12
// }

// func (rb *ringbufferP) showP() {
// 	fmt.Printf("rb: r=%d, w=%d,size=%d, count=%d,node_size=%d, magic=%d\n",
// 		*(*uint16)(unsafe.Pointer(rb.hdr.r)), *(*uint16)(unsafe.Pointer(rb.hdr.w)),
// 		*(*uint32)(unsafe.Pointer(rb.hdr.size)), *(*uint16)(unsafe.Pointer(rb.hdr.count)),
// 		*(*uint16)(unsafe.Pointer(rb.hdr.node_size)), *(*uint32)(unsafe.Pointer(rb.magic)))
// }

/*
#define KUMAP_IOC_MAGIC	'K'
#define KUMAP_IOC_SEM_WAIT _IOW(KUMAP_IOC_MAGIC, 1, int)
IOC_SEM_WAIT:
1<<30 |
'K'<<8 |
1<<0 |
4<<16
*/
/*
#define TUNGET_PAGE_SIZE _IOR('T', 230, unsigned int)
#define TUN_IOC_SEM_WAIT _IOW('T', 231, unsigned int)
*/
var IOC_SEM_WAIT int = 1<<30 | 84<<8 | 231<<0 | 4<<16 //T = 84, K = 75
type mytun_t struct {
	tund    *tuntap.Interface
	devType int
}

func main() {
	fmt.Println("IOC_SEM_WAIT:", IOC_SEM_WAIT)
	var err error
	tun := mytun_t{devType: 1}
	tun.tund, err = tuntap.Open("mytundev", tuntap.DevKind(tun.devType), false, tuntap.SetUseMytun(true))
	if err != nil {
		panic(err)
		return
	}

	var arg int

	size := 4096 * (1 << 6)
	//size := 2048
	mapbuf, err := syscall.Mmap(int(tun.tund.File().Fd()), 0, size, syscall.PROT_READ|syscall.PROT_WRITE, syscall.MAP_SHARED)
	if err != nil {
		panic(err)
	}
	fmt.Printf("gomap mapbuf len=%d\n", len(mapbuf)) //output: 2048, equal mmap len
	rb := new(ringbuffer)
	//rb.encodeP(uintptr(unsafe.Pointer(&mapbuf[0])))
	rb.encode(mapbuf)
	//rb := encodeRB(mapbuf)
	rb.show()
	//rb.test()
	//pktPool := packet.NewPktBufPool()
	//pb := packet.GetPktFromPool(pktPool)
	for {
		dataBuf := rb.GetPktData()
		if dataBuf == nil {
			r1, r2, syerr := syscall.Syscall(syscall.SYS_IOCTL, tun.tund.File().Fd(), uintptr(IOC_SEM_WAIT), uintptr(unsafe.Pointer(&arg)))
			if syerr != 0 {
				return //syscall.Errno(syerr)
			}
			_, _ = r1, r2
			fmt.Println("syscall.Syscall ioctl ok")
			rb.show()
			continue
		}
		dataLen := binary.LittleEndian.Uint16(dataBuf)
		if int(dataLen) > len(dataBuf) {
			log.Printf("dataLen=%d, len(dataBuf)=%d", dataLen, len(dataBuf))
			panic(err)
		}
		log.Printf("dataLen=%d, len(dataBuf)=%d", dataLen, len(dataBuf))
		userData := dataBuf[2:dataLen]
		if tun.devType == int(tuntap.DevTap) {
			log.Printf("DevTap DevTap DevTap DevTap")
			ether := packet.TranEther(userData)
			if ether.IsArp() {
				mylog.Info("---------arp broadcast from %s, dst mac :%s,src mac :%s", tun.tund.Name(), ether.DstMac.String(), ether.SrcMac.String())
				//log.Printf("dst mac :%s", ether.DstMac.String())
				//log.Printf("src mac :%s", ether.SrcMac.String())
			}
			if !(ether.IsArp() || ether.IsIpPtk() || ether.IsQvlanPtk()) {
				//mylog.Warning(" not arp ,and not ip packet, ether type =0x%0x%0x ===============\n", ether.Proto[0], ether.Proto[1])
				rb.ReleasePktData()
				continue
				//err = errors.New("vnetFilter")
				//return
			}
			if ether.IsIpv4Ptk() {
				iphdr, err := packet.ParseIPHeader(userData[packet.EtherSize:])
				if err != nil {
					log.Printf("ParseIPHeader err: %s\n", err.Error())
				}
				log.Println("tun read ", iphdr.String())
			}
		}
		rb.ReleasePktData()
	}
}
