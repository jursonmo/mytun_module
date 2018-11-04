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

#include "ntrack_rbf.h"

#define TUNGET_PAGE_SIZE _IOR('T', 230, unsigned int)
#define TUN_IOC_SEM_WAIT _IOW('T', 231, unsigned int)

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

int main( void )  
{  
	int tunfd, size, pageSize, mmapSize, data_len;  
	int cmd, arg; 
	rbf_t *rbf;
	char *mapBuf = NULL, *data = NULL, *user_data = NULL;
	
	char tun_name[IFNAMSIZ]; 
	tun_name[0]='\0'; 
	strcpy(tun_name,"mytundev");
	tunfd = tun_creat(tun_name, IFF_TUN|IFF_NO_PI); 
	if(tunfd<0) 
	{ 
		perror("tun_create"); 
		return -1; 
	} 
	//buffer = (char *)malloc(pageSize * (1 << rbf_order));  
	//memset(buffer, 0, );  
	pageSize = (int)(sysconf(_SC_PAGESIZE));
	mmapSize = pageSize * (1 << rbf_order);
	printf("sysconf  pageSize =%d, mmapSize=%d \n", pageSize, mmapSize);
	
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
	
	cmd =TUNGET_PAGE_SIZE;   
	printf("ioctl  ing, get page size \n");
	if (ioctl(tunfd, cmd, &arg)< 0) {
		printf("ioctl fail\n");
		return -1;
	}
	if (arg != pageSize) {
		printf("ioctl get page size =%d, sysconf  pageSize =%d, two is not equal\n", arg, pageSize);
	}
		
 	cmd =TUN_IOC_SEM_WAIT;   
  	while (1){
		printf("ioctl  ing \n");
		if (ioctl(tunfd, cmd, &arg)< 0) {
			printf("ioctl fail\n");
			return -1;
		}
	
		 while (1){
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
				printf("data_len(%d) >RBF_NODE_SIZE(%d)  \n", data_len, RBF_NODE_SIZE);
				return -1;
			}
			printf("data_len(%d)\n", data_len);
			user_data = malloc(data_len);
			memcpy(user_data, data+2, data_len);
			free(user_data);
			rbf_release_data(rbf);			
		 }		
  }

 out: 

	munmap(mapBuf, mmapSize);
	close(tunfd);
	return 0;  
}

