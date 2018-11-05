#ifndef __NTRACK_RBF_H__
#define __NTRACK_RBF_H__

#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/module.h>
#define print printk
#else /* end kernel */

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#define print printf

#endif /* __KERNEL__ */
struct ringbuf_req {
	unsigned int mem_size;
	unsigned int packet_size;
};

/* 
* ring buffer system defines. 
*/
int rbf_order = 6;
#define RBF_MAGIC 12345
#define RBF_NODE_SIZE	(1024 * 2)  //32 //1600

typedef struct ringbuffer_header {
	/* idx read, write record the idx[N] */
	volatile uint16_t r,w;

	uint32_t size;	//buffer size;
	uint16_t count;	//node count;
	uint16_t node_size;
} rbf_hdr_t;

typedef struct ringbuffer {
	rbf_hdr_t hdr;
	uint32_t magic;
	uint8_t buffer[0];
} rbf_t;

uint32_t rbf_size(rbf_t *rbf) 
{
	return rbf->hdr.size + (uint32_t)offsetof(rbf_t, buffer);//sizeof(rbf_hdr_t);
}

uint32_t rbf_buffer_size(rbf_t *rbf) 
{
	return rbf->hdr.size;
}

uint32_t rbf_node_size(rbf_t *rbf) 
{
	return rbf->hdr.node_size;
}

static inline rbf_t* rbf_init(void *mem, uint32_t size, uint16_t node_size)
{
	rbf_t *rbp = (rbf_t*)mem;
	//WARN_ON((size % L1_CACHE_BYTES) || (RBF_NODE_SIZE % L1_CACHE_BYTES));
	
	#ifdef __KERNEL__
	printk("PAGE_SIZE=%lu, sizeof(rbf_t)=%ld, sizeof(rbf_hdr_t)=%ld, offset, r=%ld, w=%ld,size=%ld,count=%ld,node_size=%ld,magic=%ld\n",PAGE_SIZE, sizeof(rbf_t), sizeof(rbf_hdr_t)
		, offsetof(rbf_hdr_t,r), offsetof(rbf_hdr_t,w), offsetof(rbf_hdr_t,size), offsetof(rbf_hdr_t,count), offsetof(rbf_hdr_t,node_size), offsetof(rbf_t,magic));
	printk(" rbf_init : L1_CACHE_BYTES =%d , size yu L1_CACHE_BYTES=%d, RBF_NODE_SIZE yu L1_CACHE_BYTES=%d \n", L1_CACHE_BYTES, size % L1_CACHE_BYTES, RBF_NODE_SIZE % L1_CACHE_BYTES);
	#endif
	memset(rbp, 0, sizeof(rbf_t));
	rbp->hdr.size = size - offsetof(rbf_t, buffer);//sizeof(rbf_hdr_t);
	if (!node_size) {
		rbp->hdr.node_size = node_size;
	}else {
		rbp->hdr.node_size = RBF_NODE_SIZE;
	}
	rbp->hdr.count = rbp->hdr.size / rbp->hdr.node_size;
	rbp->magic = RBF_MAGIC;
	if (rbf_size(rbp) != size) {
		print("fail : rbp->hdr.size =%u+ sizeof(rbf_hdr_t)=%ld, rbf_size(rbp) =%u, size=%u, not equal\n", rbp->hdr.size,  sizeof(rbf_hdr_t) , rbf_size(rbp), size);
		return NULL;
	}

	print("\n\trbf_init mem: %p\n"
		"\t hdr.size: %u,  node_size=%d, count: %u\n"
		"\tr: %d, w: %d\n", 
		mem, rbp->hdr.size, rbp->hdr.node_size, rbp->hdr.count, 
		rbp->hdr.r, rbp->hdr.w);

	return rbp;
}

static inline void rbf_dump(rbf_t *rbp)
{
	print("mem: %p, sz: 0x%x, count: 0x%x\n", 
		rbp, rbp->hdr.size, rbp->hdr.count);

	print("\tr: %d, w: %d\n", rbp->hdr.r, rbp->hdr.w);
}

static inline void rbf_dump_rw(rbf_t *rbp)
{
	print("hdr.r : %d, w: %d\n", rbp->hdr.r, rbp->hdr.w);
}

static inline void *rbf_get_buff(rbf_t* rbp)
{
	volatile uint16_t idx = (rbp->hdr.w + 1) % rbp->hdr.count;

	/* overflow ? */
	if (idx != rbp->hdr.r) {
		return (void *)&rbp->buffer[RBF_NODE_SIZE * rbp->hdr.w];
	}

	return NULL;
}

static inline void rbf_release_buff(rbf_t* rbp)
{
	rbp->hdr.w = (rbp->hdr.w + 1) % rbp->hdr.count;
}

static inline void *rbf_get_data(rbf_t *rbp)
{
	uint16_t idx = rbp->hdr.r;

	if(idx != rbp->hdr.w) {
		return (void*)&rbp->buffer[RBF_NODE_SIZE * idx];
	}

	return NULL;
}

static inline void rbf_release_data(rbf_t *rbp)
{
	rbp->hdr.r = (rbp->hdr.r + 1) % rbp->hdr.count;
}

static inline bool rbf_have_data(rbf_t *rbp)  
{
	return  rbp->hdr.r !=rbp->hdr.w;
}

static inline bool rbf_have_buff(rbf_t* rbp)
{
	return ((rbp->hdr.w + 1) % rbp->hdr.count) != rbp->hdr.r;
}


#endif /* __NTRACK_RBF_H__ */
