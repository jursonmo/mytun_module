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

### 现象
  从pprof 看，写的系统调用比读的系统调用要更加耗时，为啥，因为tun fd的读read是从 sk_receive_queue直接读数据的，如果没有数据可读，schedule(),这样后的cpu 耗时就不会统计到read 的系统调用里了，也就是说read的系统调用耗时统计的是陷入内核、拷贝数据到应用层消耗cpu的时间。但是tun fd write 就不一样了，write时，相当于tun 网卡接受到数据并且走协议栈处理，这个过程消耗的cpu时间都算到write系统调用里，所以写的系统调用比读的系统调用要更加耗时。(linux 4.11.x 版本后，增加 tun_rx_batched、 linux 4.15.x 开始增加了tun napi 功能 ，都可以缓解tun_fd write 占用CPU 太多的问题)

解决方法:
  1. tun fd write 时也创建一个缓存队列，write是只需把数据放到队列里并创建taskle任务后可以返回到应用层，内核会用taskle去发送这个队列的数据(参考ifb)。这样写系统调用就不会显示太耗时。
  2. 在tun 网卡上开启rps,  把部分软中断的消耗分摊到其他cpu上，但是这个只有多流测试的时候才可能分摊，因为是根据五元组hash分摊的，且分摊的效果不确定。
  3. 像 vhost 那样起个内核线程来接受应用层的数据并将数据往内核协议栈上送，这样write tap 时不需要write系统调用，eventfd通知 就行。
  
### TODO
1. use tasklet to put tx_ringbuf packet to protocol stack(reference ifb)
2. put skb hash(from skb_get_hash()) to node_data, then userspace can use the skb hash to do balance
3. ringBuffer  should cache_line_padding(经常并发读写的两个变量不要在同一个cache line, 避免伪共享的方法是，cache_line_padding 隔开两个变量， c语言里可以用union 避免false sharing)
4. 或者借鉴vhost 的方式来避免每次往tun/tap文件描述符读写数据的系统调用开销：创建一个内核线程来读取tun/tap sk_receive_queue的数据到环形缓冲区，用eventfd通知应用层读取环形缓冲区的数据。[vhost及与tap关联](https://github.com/jursonmo/articles/blob/master/record/kvm-qemu/vhost%E5%8F%8A%E4%B8%8Etap%E5%85%B3%E8%81%94.md)

