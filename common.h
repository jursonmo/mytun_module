#ifndef __COMMON_H__
#define __COMMON_H__

#define TUNGET_PAGE_SIZE _IOR('T', 230, unsigned int)
#define TUN_IOC_SEM_WAIT _IOW('T', 231, unsigned int)
#define  TUNSET_RBF  _IOW('T', 232, unsigned int)

#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/module.h>
#define print printk

#define KUMAP_LOG_LEVEL	2
#define KUMAP_LOG(level, fmt, ...) do { \
	if ((level) <= KUMAP_LOG_LEVEL) { \
		printk("*KUMAP* " fmt "\n", ##__VA_ARGS__); \
	} \
} while (0)

#define KUMAP_LOG_IF(level, cond, fmt, ...) do { \
	if ((level) <= KUMAP_LOG_LEVEL) { \
		if (cond) { \
			printk("*KUMAP* " fmt "\n", ##__VA_ARGS__); \
		} \
	} \
} while (0)


#define KUMAP_ASSERT(cond)	BUG_ON(!(cond))

#define KUMAP_ASSERT_MSG(cond, fmt, ...) do { \
	if (unlikely(!(cond))) { \
		printk(fmt "\n", ##__VA_ARGS__); \
		BUG(); \
	} \
} while (0)

	
#define KUMAP_ERROR(...)			KUMAP_LOG(0, ##__VA_ARGS__)
#define KUMAP_ERROR_IF(cond, ...)	KUMAP_LOG_IF(0, cond, ##__VA_ARGS__)
	
#define KUMAP_WARN(...)			KUMAP_LOG(1, ##__VA_ARGS__)
#define KUMAP_WARN_IF(cond, ...)	KUMAP_LOG_IF(1, cond, ##__VA_ARGS__)
	
#define KUMAP_INFO(...)			KUMAP_LOG(2, ##__VA_ARGS__)
#define KUMAP_INFO_IF(cond, ...)	KUMAP_LOG_IF(2, cond, ##__VA_ARGS__)
	
#define KUMAP_DEBUG(...)			KUMAP_LOG(3, ##__VA_ARGS__)
#define KUMAP_DEBUG_IF(cond, ...)	KUMAP_LOG_IF(3, cond, ##__VA_ARGS__)

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

#endif /* __COMMON_H__ */
