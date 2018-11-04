# mytun_module
a tun module with mmap, reduce system call for higher performent

###   higher performent, some way :
1. single cpu:  use this mytun moudule to reduce read system call
2.  smp : use tun multiqueue and open RPS/RFS 

### TODO
1. reduce write system call
