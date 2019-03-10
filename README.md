# mytun_module
a tun module with mmap, reduce system call for higher performent. ( learn from packet_mmap ringbuf, tpacket_rcv)

###   higher performent, some way :
1. single cpu:  use this mytun moudule to reduce read system call
2.  smp : use tun multiqueue and open RPS/RFS 

### what this module have done
1. Linux original Tun/tap has been built-in.  I don't want to recompile the kernel in order to turn Tun into an insertable kernel module. so i make another tun/tap module name "mytun", it can a Loadable module and can be work with original Tun/tap in kernel.if you want to use mytun, shoule insmmod mytun.ko first, and open ("/dev/net/mytun",O_RDWR) instead of open ("/dev/net/tun",O_RDWR),see example.c
2. Set ringbuf size through ioctl
3. Synchronization between kernel and user layer through IOCTL or poll
4. reduce write system call: use tx_ringbuf send multiple packet in one write system call
### TODO
1. use tasklet to put tx_ringbuf packet to protocol stack
2. put skb hash(from skb_get_hash()) to node_data, then userspace can use the skb hash to do balance
3. ringBuffer  should cache_line_padding(经常并发读写的两个变量不要在同一个cache line, 避免伪共享的方法是，cache_line_padding 隔开两个变量)

