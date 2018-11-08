# mytun_module
a tun module with mmap, reduce system call for higher performent

###   higher performent, some way :
1. single cpu:  use this mytun moudule to reduce read system call
2.  smp : use tun multiqueue and open RPS/RFS 

### what this module have done
1. Linux original Tun/tap has been built-in.  I don't want to recompile the kernel in order to turn Tun into an insertable kernel module. so i make another tun/tap module name "mytun", it can a Loadable module and can be work with original Tun/tap in kernel.if you want to use mytun, shoule insmmod mytun.ko first, and open ("/dev/net/mytun",O_RDWR) instead of open ("/dev/net/tun",O_RDWR),see example.c
2. Set ringbuf size through ioctl
3. Synchronization between kernel and user layer through IOCTL or poll
### TODO
1. reduce write system call
