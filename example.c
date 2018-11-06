#include <stdio.h>  
#include <sys/types.h> 
#include <fcntl.h>  
#include <sys/mman.h>  
#include <stdlib.h>  
#include <string.h>  
#include <linux/ioctl.h>
#include <sys/un.h>  
#include <sys/socket.h>  
#include <stddef.h> 
#include <errno.h>
#include <linux/if_tun.h> 
#include <linux/if.h> 
#include <stdbool.h>

#include<poll.h>

#include "ntrack_rbf.h"


int tun_creat(char *dev, int flags) 
{ 
	 struct ifreq ifr; 
	 int fd,err; 
	 if (!dev) {
		return -1;
	 }
	 if((fd = open ("/dev/net/mytun",O_RDWR))<0) //you can replace it to tap to create tap device. 
	  return fd; 
	 memset(&ifr,0,sizeof (ifr)); 
	 ifr.ifr_flags|=flags; 
	 if(*dev != '\0') 
	  strncpy(ifr.ifr_name, dev, IFNAMSIZ); 
	 if((err = ioctl(fd, TUNSETIFF, (void *)&ifr))<0) 
	 { 
	  close (fd); 
	  return err; 
	 } 
	 strcpy(dev,ifr.ifr_name); 
	 return fd; 
}

static int handle_data(rbf_t *rbf)
{
	int data_len = 0, total_len = 0;
	char *user_data = NULL, *data = NULL;
	while (1) {
	   data = NULL;
	   //printf("rbf_get_data starting \n");
	   //sleep(1);
	   data = rbf_get_data(rbf);
	   if (!data) {
		   printf("no data\n");
		   break;
	   }
	   //printf("rbf_get_data end \n");
	   memcpy(&data_len, data, 2);
	   if (data_len > RBF_NODE_SIZE) {
		   printf("data_len(%d) >RBF_NODE_SIZE(%d)	\n", data_len, RBF_NODE_SIZE);
		   return -1;
	   }
	   printf("data_len(%d)\n", data_len);
	   user_data = malloc(data_len - 2);
	   memcpy(user_data, data+2, data_len-2);
	   //todo: handle user_data
	   free(user_data);
	   rbf_release_data(rbf);
	   total_len += data_len;
	}
	return total_len;
}

int main( int argc, char **argv )  
{  
	int tunfd, size = 0, pageSize = 0, mmapSize = 0;  
	int cmd, arg, use_ioctl = 0, use_poll= 0; 
	rbf_t *rbf;
	char *mapBuf = NULL;
	int ret;
	
	if (argc != 2) {
		printf("argc must == 2:  1 means  use_ioctl,  2 means  use_poll\n" );
		return -1;
	}
	printf("argc =%d,  argv[0] =%s,  \n", argc, argv[0] );
	if (atoi(argv[1]) == 1) {
		use_ioctl = 1;
	}else if (atoi(argv[1]) == 2) {
		use_poll = 1;
	}else {
		printf(" 1 means  use_ioctl,  2 means  use_poll\n" );
		//return -1;
	}
	
	char tun_name[IFNAMSIZ]; 
	tun_name[0]='\0'; 
	strcpy(tun_name,"mytundev");
	tunfd = tun_creat(tun_name, IFF_TUN|IFF_NO_PI); 
	if(tunfd<0) 
	{ 
		perror("tun_create"); 
		return -1; 
	} 
	if (!use_ioctl && !use_poll) {
		printf(" normal mode \n" );
		char buf[2048];
		while(1) {
			ret = read(tunfd, buf, sizeof(buf));
			if (ret <0 )  {
				perror("read");
				return ret;
			}
			printf("read %d bytes\n", ret); 
			//TODO:handle buffer
		}
	}
	
	//testing
	ret = setsockopt(tunfd, 1, 2, NULL, 0);
	if (ret < 0) {
		perror("  setsockopt fail \n");
		//return ret;
	}
	
	mmapSize = 5000;
	struct ringbuf_req req;
	req.mem_size = mmapSize;
	req.packet_size = 2048;
	if (ioctl(tunfd, TUNSET_RBF, &req)< 0) {
		printf("ioctl set ringbuf fail \n"); 
		return -1;
	}

	if (!mmapSize) {
		pageSize = (int)(sysconf(_SC_PAGESIZE));
		mmapSize = pageSize * (1 << rbf_order);
		printf("sysconf  pageSize =%d, mmapSize=%d \n", pageSize, mmapSize);
		printf("ioctl  ing, get page size \n");
		if (ioctl(tunfd, TUNGET_PAGE_SIZE, &arg)< 0) {
			printf("ioctl fail\n");
			return -1;
		}
		if (arg != pageSize) {
			printf("ioctl get page size =%d, sysconf  pageSize =%d, two is not equal\n", arg, pageSize);
		}
	}
	
	mapBuf = mmap(NULL, mmapSize, PROT_READ|PROT_WRITE, MAP_SHARED, tunfd, 0);
	if (!mapBuf){
		perror("mmap"); 
		return -1; 
	}
	rbf = (rbf_t *)mapBuf;
	if (rbf->magic != RBF_MAGIC) {
		printf("rbf->magic =%d\n", rbf->magic);
		return -1;
	}
	
		
 	cmd =TUN_IOC_SEM_WAIT;   
  	while (use_ioctl){
		printf("ioctl  ing \n");
		if (ioctl(tunfd, cmd, &arg)< 0) {
			printf("ioctl fail\n");
			return -1;
		}
		if (handle_data(rbf) <0 ) {
			break;
		}
  	}

	if (use_poll){
		struct pollfd pollfds;
		int timeout;
		
		timeout = 5000;
		pollfds.fd = tunfd;	
		pollfds.events = POLLIN|POLLPRI;
		for(;;){
			switch(poll(&pollfds, 1, timeout)){
			case -1:	
				printf("poll error \r\n");
				if (errno != EINTR) {
					return -1;
				}
			break;
			case 0:
				printf("time out \r\n");
				if (rbf_have_data(rbf)) {
				   printf("oh, there is  data in ringbuf, something wrong ?\n");//if timeout, before back to userspace, kernel also will poll to check if there have events, and return evnets .
				}
				rbf_dump_rw(rbf);
			break;
			default:	
				printf("sockfd have some event \r\n");
				printf("event value is 0x%x \r\n", pollfds.revents);
				if (handle_data(rbf) <0 ) {
					goto out;
				}
			break;
			}
		}
	}
 out: 

	munmap(mapBuf, mmapSize);
	close(tunfd);
	return 0;  
}

