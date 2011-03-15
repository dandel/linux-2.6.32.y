/***************************************************************************** 
 * common.h
 * 
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Description: commmon head file for memalloc char driver
 *
 * Author:
 *     Sololz <sololz@infotm.com>
 *      
 * Revision History: 
 * ­­­­­­­­­­­­­­­­­ 
 * 1.1  12/10/2009 Sololz
 * 	Create this memalloc device driver, using dynamic allocation
 * 	method.
 * 1.2	07/07/2010 Sololz
 * 	Considering dynamic allocation will cause memory fragments, so
 * 	last we decide to reserve memroy for Android media player, and 
 * 	display. But this preversion functionality memalloc is not 
 * 	abandoned, if reserved memory is oom, dynamic allocation will
 * 	method will be used.
 ******************************************************************************/

#define MEMALLOC_MAX_OPEN	256	/* max instance */
#define MEMALLOC_MAX_ALLOC	2048	/* max allocation each open */
#define MEMALLOC_DYNAMIC_MAJOR	0	/* 0 means dynamic alloc by default */
#define MEMALLOC_DYNAMIC_MINOR	255
#define	MEMALLOC_DEFAULT_MAJOR	111
#define MEMALLOC_DEFAULT_MINOR	111

#define IMAPX200_MEMORY_START	0x40000000

#define MEMALLOC_MAX_ALLOC_SIZE	(8 * 1024 * 1024)	/* 8MB is quite so enough, normally less then 3MB */

/* RESERVED MEMORY */
#if defined(CONFIG_IMAP_MEMALLOC_MANUAL_RESERVE)
#define MEMALLOC_MEM_START	IMAPX200_MEMORY_START	/* using board provide by infotm, memory start address must be this */
#define MEMALLOC_MEM_END	(MEMALLOC_MEM_START + (CONFIG_IMAP_MEMALLOC_TOTAL_SIZE * 1024 * 1024))
#define MEMALLOC_RSV_SIZE	(CONFIG_IMAP_MEMALLOC_RSV_SIZE * 1024 * 1024)	/* reserved size to be fix */
#define MEMALLOC_RSV_ADDR	(MEMALLOC_MEM_END - MEMALLOC_RSV_SIZE)
#endif	/* CONFIG_IMAP_MEMALLOC_MANUAL_RESERVE */

/* MPDMA MACROS */
#define MPDMA_IDLE		0x00000000
#define MPDMA_USED		0x34857912

/*
 * debug macros include debug alert error
 */
#ifdef CONFIG_IMAP_MEMALLOC_DEBUG
#define MEMALLOC_DEBUG(debug, ...)	\
	printk(KERN_DEBUG "%s line %d: " debug, __func__, __LINE__, ##__VA_ARGS__)
#else
#define MEMALLOC_DEBUG(debug, ...)  	do{}while(0)
#endif /* CONFIG_IMAP_MEMALLOC_DEBUG */

#define MEMALLOC_ALERT(alert, ...)	\
	printk(KERN_ALERT "%s line %d: " alert, __func__, __LINE__, ##__VA_ARGS__)
#define MEMALLOC_ERROR(error, ...)	\
	printk(KERN_ERR "%s line %d: " error, __func__, __LINE__, ##__VA_ARGS__)

#define memalloc_debug(debug, ...)	MEMALLOC_DEBUG(debug, ##__VA_ARGS__)
#define memalloc_alert(alert, ...)	MEMALLOC_ALERT(alert, ##__VA_ARGS__)
#define memalloc_error(error, ...)	MEMALLOC_ERROR(error, ##__VA_ARGS__)

/*
 * ioctl commands
 */
#define MEMALLOC_MAGIC  	'k'

#define MEMALLOC_GET_BUF        _IOWR(MEMALLOC_MAGIC,	1, unsigned long)
#define MEMALLOC_FREE_BUF       _IOW(MEMALLOC_MAGIC,	2, unsigned long)

#define MEMALLOC_SHM_GET	_IOWR(MEMALLOC_MAGIC,	3, unsigned long)	/* shared memory is strongly not recommended to be used */
#define MEMALLOC_SHM_FREE	_IOW(MEMALLOC_MAGIC,	4, unsigned long)

#define MEMALLOC_GET_MPDMA_REG	_IOWR(MEMALLOC_MAGIC,	5, unsigned long)
#define MEMALLOC_FLUSH_RSV	_IO(MEMALLOC_MAGIC,	6)

#define MEMALLOC_MPDMA_COPY	_IOWR(MEMALLOC_MAGIC,	7, unsigned long)	/* use memory pool in kernel space */
#define MEMALLOC_LOCK_MPDMA	_IO(MEMALLOC_MAGIC,	8)			/* lock memory pool hard ware resource */
#define MEMALLOC_UNLOCK_MPDMA	_IO(MEMALLOC_MAGIC,	9)			/* unlock memory pool hard ware resource */
#define MEMALLOC_GET_MPDMA_MARK	_IOWR(MEMALLOC_MAGIC,	10, unsigned long)

/* ... more to come */
#define MEMALLOC_RESET		_IO(MEMALLOC_MAGIC, 15) /* debugging tool */
#define MEMALLOC_MAX_CMD	15

/* memory pool dma registers */
typedef struct {
	volatile unsigned int	ch0_maddr;
	volatile unsigned int	ch1_maddr;
	volatile unsigned int	ch2_maddr;
	volatile unsigned int	ch3_maddr;
	volatile unsigned int	dma_en;
	volatile unsigned int	int_en;
	volatile unsigned int	int_stat;
	volatile unsigned char	resv[0x64];
	volatile unsigned int	ch0_saddr;
	volatile unsigned int	ch0_ctrl;
	volatile unsigned int	ch1_saddr;
	volatile unsigned int	ch1_ctrl;
	volatile unsigned int	ch2_saddr;
	volatile unsigned int	ch2_ctrl;
	volatile unsigned int	ch3_saddr;
	volatile unsigned int	ch3_ctrl;
}mpdma_reg_t;

/*
 * for ioctl 
 */
typedef struct
{
	unsigned int	paddr;
	unsigned int	size;
	/* unsigned int	vaddr; */
}memalloc_param_t;

/* 
 * share memory structure 
 */
typedef struct
{
	/* 
	 * record the number of open handle using this share memory part, 
	 * this count mark increases at each share memory get ioctl syscall.
	 * and decrease at each share momory free ioctl syscall. also if 
	 * user forget to free this sharing memory, this driver should recycle
	 * it if not been using.
	 */
	unsigned int	dep_count;
	unsigned int	shaddr;
	unsigned int	size;
	unsigned int	key;
}shm_param_t;

/*
 * this structure stores global parameters
 */
typedef struct
{
        int     	major;
        int     	minor;
        int     	inst_count;

	int		sh_count;	/* for android share memory in different processes and threads */
	shm_param_t	shm[MEMALLOC_MAX_ALLOC];
}memalloc_global_t;

/*
 * this structure for open instance
 */
typedef struct
{
        unsigned int   	paddr[MEMALLOC_MAX_ALLOC];	/* stores physics address alloc by ioctl for each instance */
	int		alloc_count;

	unsigned int	shaddr[MEMALLOC_MAX_ALLOC];
}memalloc_inst_t;

/*
 * reserved memory list structure, every memory node start address
 * is the end address + 1 of pre node memory, current end memory 
 * address is the start address of next node memory start address - 1.
 * it means that all memory block in list is linear address, this 
 * design will be very easy for unused block merge. considering this
 * memory alloc might use O(n) time to get correspond memory, TODO, 
 * to be optimized.
 */
typedef struct rsv_mem_struct
{
	unsigned int		mark;	/* mark for current memory block is used or not */
	unsigned int		phys;	/* physical start address of current memory block */
	unsigned int		*virt;	/* kernel space virtual start address of curretn memory block */
	unsigned int		size;	/* current memory block size */
	struct rsv_mem_struct	*pre;	/* pre memory node of current memory block */
	struct rsv_mem_struct	*next;	/* next memory node of current memory block */
}rsv_mem_t;

/*
 * memory pool copy transfer structure between user space and 
 * kernel.
 */
typedef struct 
{
	unsigned int	*vsrc;	/* source virtual address in user space */
	unsigned int	psrc;	/* source physical address */
	unsigned int	*vdst;	/* dest virtual address in user space */
	unsigned int	pdst;	/* dest physical address */
	unsigned int	size;	/* copy size */
}mpdma_param_t;
