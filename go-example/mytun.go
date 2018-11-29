package main

import (
	"encoding/binary"
	"fmt"
	"log"
	"packet"
	"syscall"
	"time"
	"unsafe"

	"github.com/jursonmo/go-tuntap/tuntap"
)

var (
	QvlanPtk      [2]byte = [...]byte{0x81, 0x00}
	Ipv6Ptk       [2]byte = [...]byte{0x86, 0xdd}
	IpPtk         [2]byte = [...]byte{0x08, 0x00}
	ArpPkt        [2]byte = [...]byte{0x08, 0x06}
	BroadcastAddr [6]byte = [6]byte{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
	EtherSize             = 14
)

type MAC [6]byte
type Packet []byte

type Ether struct {
	DstMac MAC
	SrcMac MAC
	Proto  [2]byte
}

func TranEther(b []byte) *Ether {
	/*
		p := Packet(b)
		return &Ether{
			DstMac : p.GetDstMac(),
			SrcMac : p.GetSrcMac(),
			Proto  : p.GetProto(),
		}
	*/
	return (*Ether)(unsafe.Pointer(&b[0]))
}
func (e *Ether) GetProto() uint16 {
	return uint16(e.Proto[0])<<8 | uint16(e.Proto[1])
}

func (e *Ether) IsBroadcast() bool {
	return e.DstMac == BroadcastAddr
}
func (e *Ether) IsArp() bool {
	return e.Proto == ArpPkt
}

func (e *Ether) IsQvlanPtk() bool {
	return e.Proto == QvlanPtk
}

func (e *Ether) IsIpPtk() bool {
	return e.Proto == IpPtk || e.Proto == Ipv6Ptk
}

func (e *Ether) IsIpv4Ptk() bool {
	return e.Proto == IpPtk
}

func (e *Ether) IsIpv6Ptk() bool {
	return e.Proto == Ipv6Ptk
}

func (p *Packet) GetDstMac() MAC {
	return MAC{(*p)[0], (*p)[1], (*p)[2], (*p)[3], (*p)[4], (*p)[5]}
}
func (p *Packet) GetSrcMac() MAC {
	return MAC{(*p)[6], (*p)[7], (*p)[8], (*p)[9], (*p)[10], (*p)[11]}
}
func (p *Packet) GetProto() [2]byte {
	return [2]byte{(*p)[12], (*p)[13]}
}
func (p *Packet) IsBroadcast() bool {
	return p.GetDstMac() == BroadcastAddr
}
func (p *Packet) IsArp() bool {
	return p.GetProto() == ArpPkt
}
func (p *Packet) IsIpPtk() bool {
	return p.GetProto() == IpPtk
}

func (m MAC) String() string {
	return fmt.Sprintf("%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5])
}
func PrintMac(m MAC) string {
	return fmt.Sprintf("%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5])
}

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

const (
	MAGIC = 12345
)

type ringbuffer_header struct {
	r, w             uint16
	size             uint32
	count, node_size uint16
	magic            uint32
}

type ringbuffer struct {
	hdr    *ringbuffer_header
	buffer []byte
}

func (rb *ringbuffer) encode(mapbuf []byte) {
	rb.hdr = (*ringbuffer_header)(unsafe.Pointer(&mapbuf[0]))
	if rb.hdr.magic != MAGIC {
		fmt.Printf("rb.hdr.magic =%d != 12345\n", rb.hdr.magic)
		panic("rb.hdr.magic != 12345")
	}
	offset := unsafe.Sizeof(ringbuffer_header{})
	if offset != 16 {
		fmt.Printf("offset =%d != 16\n", offset)
		panic("Offsetof(rb.buffer) != 16")
	}
	rb.buffer = mapbuf[offset:]
}

func (rb *ringbuffer) GetPktData() []byte {
	if rb.hdr.r == rb.hdr.w {
		return nil
	}
	offset := int(rb.hdr.r) * int(rb.hdr.node_size)
	return rb.buffer[offset : offset+int(rb.hdr.node_size)]
}

func (rb *ringbuffer) ReleasePktData() {
	rb.hdr.r = (rb.hdr.r + 1) % rb.hdr.count
	//binary.LittleEndian.PutUint16(rb.mapBuf, rb.hdr.r)
}

func (rb *ringbuffer) show() {
	fmt.Printf("rb show : r=%d, w=%d,size=%d, count=%d,node_size=%d, magic=%d, len(buffer)=%d\n", rb.hdr.r,
		rb.hdr.w, rb.hdr.size, rb.hdr.count, rb.hdr.node_size, rb.hdr.magic, len(rb.buffer))
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
	rb.encode(mapbuf)
	rb.show()

	nodataNum := 0
	for {
		dataBuf := rb.GetPktData()
		if dataBuf == nil {
			//阻塞式系统调用syscall 会导致M休眠，会创建新的系统线程来执行P
			//如果连续睡眠次数小于10，就通过就睡眠1毫秒来等待数据到来，没有数据导致的连续睡眠次数超过10，说明数据不多，就用syscall ioctl 来等待数据到来
			if nodataNum < 10 {
				nodataNum++
				time.Sleep(time.Millisecond)
				continue
			}
			r1, r2, syerr := syscall.Syscall(syscall.SYS_IOCTL, tun.tund.File().Fd(), uintptr(IOC_SEM_WAIT), uintptr(unsafe.Pointer(&arg)))
			if syerr != 0 {
				return //syscall.Errno(syerr)
			}
			_, _ = r1, r2
			fmt.Println("syscall.Syscall ioctl ok")
			rb.show()
			continue
		}
		nodataNum = 0 //重置

		dataLen := binary.LittleEndian.Uint16(dataBuf)
		if int(dataLen) > len(dataBuf) {
			log.Printf("dataLen=%d, len(dataBuf)=%d", dataLen, len(dataBuf))
			panic(err)
		}
		log.Printf("dataLen=%d, len(dataBuf)=%d", dataLen, len(dataBuf))
		userData := dataBuf[2:dataLen]
		if tun.devType == int(tuntap.DevTap) {
			ether := TranEther(userData)
			if ether.IsArp() {
				log.Printf("---------arp broadcast from %s, dst mac :%s,src mac :%s", tun.tund.Name(), ether.DstMac.String(), ether.SrcMac.String())
			}
			if !(ether.IsArp() || ether.IsIpPtk() || ether.IsQvlanPtk()) {
				log.Printf(" not arp ,and not ip packet, ether type =0x%0x%0x ===============\n", ether.Proto[0], ether.Proto[1])
				rb.ReleasePktData()
				continue
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
