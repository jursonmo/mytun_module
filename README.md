# mytun_module
a tun module with mmap, reduce system call for higher performent. ( learn from packet_mmap ringbuf, tpacket_rcv)

###   higher performent, some way :
1. single cpu:  use this mytun moudule to reduce read system call
2.  smp : use tun multiqueue and open RPS/RFS 

### what this module have done
1. Linux original Tun/tap has been built-in.  I don't want to recompile the kernel in order to turn Tun into an insertable kernel module. so i make another tun/tap module name "mytun", it can a Loadable module and can be work with original Tun/tap in kernel.if you want to use mytun, shoule insmmod mytun.ko first, and open ("/dev/net/mytun",O_RDWR) instead of open ("/dev/net/tun",O_RDWR),see example.c
2. Set ringbuf size through ioctl, use rx_ringbuf to get multiple packet in one read system call
3. Synchronization between kernel and user layer through IOCTL or poll
4. reduce write system call: use tx_ringbuf send multiple packet in one write system call
5. no use rx_ringbuf, just get multiple packets from sk_receive_queue and put the packets to userspace in one read syscall

### TODO
1. use tasklet to put tx_ringbuf packet to protocol stack
2. put skb hash(from skb_get_hash()) to node_data, then userspace can use the skb hash to do balance
3. ringBuffer  should cache_line_padding(经常并发读写的两个变量不要在同一个cache line, 避免伪共享的方法是，cache_line_padding 隔开两个变量)

### 现象
  从pprof 看，写的系统调用比读的系统调用要更加耗时，为啥，因为tun fd的读read是从 sk_receive_queue直接读数据的，如果没有数据可读，schedule(),这样后的cpu 耗时就不会统计到read 的系统调用里了，也就是说read的系统调用耗时统计的是陷入内核、拷贝数据到应用层消耗cpu的时间。但是tun fd write 就不一样了，write时，相当于tun 网卡接受到数据并且走协议栈处理，这个过程消耗的cpu时间都算到write系统调用里，所以写的系统调用比读的系统调用要更加耗时。
  解决方法，tun fd write 时也创建一个缓存队列，write是只需把数据放到队列里并创建taskle任务后可以返回到应用层，内核会用taskle去发送这个队列的数据。这样写系统调用就不会显示太耗时。
