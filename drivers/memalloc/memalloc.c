/***************************************************************************** 
 * memalloc.c 
 * 
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Description: main file of memalloc char driver to allocate physical
 * address for multi-media.
 *
 * Author:
 *     Sololz <sololz@infotm.com>
 *      
 * Revision History: 
 * ­­­­­­­­­­­­­­­­­ 
 * 1.1  2009/12/10 Sololz
 * 	create driver structure, support physical buffer allocation.
 * 1.2  2009/12/25 Raymond
 *	add memalloc_mmap.
 * 1.3  2010/06/02 Sololz
 * 	add share memory support for Android, and modify driver structure.
 * 1.4  2010/06/13 Sololz
 * 	add memory pool operation to support DMA copy.
 * 1.5	2010/07/07 Sololz
 * 	add reserved memory management for Android to avoid memory fragment.
 * 1.6	2010/07/15 Sololz
 * 	modified mmap mode, force mmap user space memory to uncached.
 * 1.7	2010/07/21 Sololz
 * 	add function export for other driver use.
 * 1.8	2010/07/26 Sololz
 * 	add support for user space virtual address memory copy using mmpool.
 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>

#include <plat/mem_reserve.h>
#include <mach/imapx_base_reg.h>
#include <mach/imapx_sysmgr.h>

#include "common.h"
#include "lpmemalloc.h"

/* GLOBAL VARIABLES */
static struct mutex	*m_mutex	= NULL;
static struct class	*m_class	= NULL;
static void __iomem	*mpdma_reg	= NULL;
static rsv_mem_t	*rsv_head	= NULL;	/* head of reserved memory list */
static unsigned int	rsv_phys	= 0;	/* reserved memory start physical address */
static unsigned int	rsv_phys_end	= 0;	/* reserved memory end physical address */
static unsigned int	rsv_size	= 0;	/* reserved memory size */
static unsigned int	*rsv_virt	= NULL;	/* reserved memory start virtual address */
static unsigned int	export_mark	= 0;	/* mark for export function validation */
static memalloc_inst_t	export_inst;
#if 0
static unsigned int	mpdma_mark	= 0; 	/* mark for mpdma used or not */
static unsigned int	*mpmark		= NULL;
#endif

#ifdef CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE
static unsigned int	trace_memory	= 0;	/* trace memory usage */
#endif	/* CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE */

static memalloc_global_t m_global;		/* pre driver version global variables pack */

/* INTERNAL FUNCTION DECLARATION */
static int m_reset_mem(memalloc_inst_t *inst);
static int m_insert_mem(unsigned int paddr, memalloc_inst_t *inst);
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
static int m_free_mem(unsigned int paddr, memalloc_inst_t *inst);
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */

static int m_enable_mp(void);
static int m_disable_mp(void);

static int rsv_alloc(unsigned int size, unsigned int *paddr, memalloc_inst_t *inst);
static int rsv_free(unsigned int paddr, memalloc_inst_t *inst);

static int mpdma_memcpy(mpdma_param_t *mpdma);	/* FIXME: this function is declared to be static, this may be exported in the future */

/*
 * FUNCTION
 * memalloc_open()
 *
 * System call open of memalloc, /dev/memalloc can be open 256 times
 */
static int memalloc_open(struct inode *inode, struct file *file)
{
	int		ret	= 0;
	memalloc_inst_t	*inst	= NULL;

	mutex_lock(m_mutex);
	
	if(m_global.inst_count == MEMALLOC_MAX_OPEN)
	{
		memalloc_error("More than %d times have been openned, no more instances\n", MEMALLOC_MAX_OPEN);
		ret = -EPERM;
		goto open_return;
	}

	inst = NULL;
	inst = (memalloc_inst_t *)kmalloc(sizeof(memalloc_inst_t), GFP_KERNEL);
	if(inst == NULL)
	{
		memalloc_error("kmalloc for instance error\n");
		ret = -ENOMEM;
		goto open_return;
	}
	memset(inst, 0x00, sizeof(memalloc_inst_t));

	file->private_data = inst;
	m_global.inst_count++;
	memalloc_debug("Memalloc open OK\n");
	
open_return:
	mutex_unlock(m_mutex);
	return ret;
}

/*
 * FUNCTION
 * memalloc_release()
 *
 * All memory block should be released before memalloc_release If you 
 * forget to, memalloc_release will check it and fix your mistake
 */
static int memalloc_release(struct inode *inode, struct file *file)
{
	int 		icount	= 0;
	int		gcount	= 0;
	memalloc_inst_t	*inst	= NULL;

	mutex_lock(m_mutex);

	inst = (memalloc_inst_t *)file->private_data;

	/* check share memory whether has been freed */
	for(icount = 0; icount < MEMALLOC_MAX_ALLOC; icount++)
	{
		if(inst->shaddr[icount] != 0)
		{
			/* find this share memory in global share memory library */
			for(gcount = 0; gcount < MEMALLOC_MAX_ALLOC; gcount++)
			{
				if(m_global.shm[gcount].shaddr == inst->shaddr[icount])
				{
					if(m_global.shm[gcount].dep_count > 0)
						(m_global.shm[gcount].dep_count)--;
					else
						m_global.shm[gcount].dep_count = 0;

					if(m_global.shm[gcount].dep_count == 0)
					{
						kfree((unsigned long *)phys_to_virt(m_global.shm[gcount].shaddr));
						memset(&(m_global.shm[gcount]), 0x00, sizeof(shm_param_t));
					}
				}
			}

			inst->shaddr[icount] = 0;
		}
	}

	/* free memalloc instance structure */
	if(inst == NULL)
	{
		/* process nothing */
		memalloc_debug("file private data already NULL\n");
	}
	else
	{
		/* check whether allocated memory release normally, it's MUST step */
		m_reset_mem(inst);

		kfree(inst);
	}
	m_global.inst_count--;
	memalloc_debug("Memalloc release OK\n");

	mutex_unlock(m_mutex);

	return 0;
}

/*
 * don't forget to call free ioctl if you have called any allocate ops
 */
static int memalloc_ioctl(struct inode *inode, \
		struct file *file, \
		unsigned int cmd, \
		unsigned long arg)
{
	int			i	= 0;
	int			j	= 0;
	int 			ret	= 0;
	void 			*vaddr	= NULL;
	memalloc_inst_t		*inst	= NULL;
	memalloc_param_t	param;
	shm_param_t		shm;
	mpdma_param_t		mpdma;
	unsigned int		key = 0;

	/* check system call */
	if((inode == NULL) || (file == NULL) || (arg == 0))
	{
		memalloc_debug("memalloc ioctl system call error\n");
		ret = -EFAULT;
		goto ioctl_return;
	}

	/* check command */
	if(_IOC_TYPE(cmd) != MEMALLOC_MAGIC)
	{
		ret = -ENOTTY;
		goto ioctl_return;
	}
	if(_IOC_NR(cmd) > MEMALLOC_MAX_CMD)
	{
		ret = -ENOTTY;
		goto ioctl_return;
	}

	if(_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));

	if(ret)
	{
		ret = -EFAULT;
		goto ioctl_return;
	}

	inst = (memalloc_inst_t *)file->private_data;
	if(inst == NULL)
	{
		memalloc_error("ioctl system call error file\n");
		ret = -EFAULT;
		goto ioctl_return;
	}

	mutex_lock(m_mutex);

	switch(cmd)
	{
		/*
		 * reset current instance memory blocks
		 */
		case MEMALLOC_RESET:
			m_reset_mem(inst);
			break;

		/*
		 * memory alloc by kmalloc with flags GFP_DMA | GFP_ATOMIC
		 * the max size may be 8MB, 16MB, 32MB, you can change or
		 * get this in kernel configuration
		 */
		case MEMALLOC_GET_BUF:
			if(inst->alloc_count >= MEMALLOC_MAX_ALLOC)
			{
				memalloc_error("no more allocation allowed in this instance\n");
				ret = -ENOMEM;
				goto ioctl_return;
			}

			if(copy_from_user(&param, (memalloc_param_t *)arg, sizeof(memalloc_param_t)))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			/* check parameters */
			if((param.size == 0) || (param.size > MEMALLOC_MAX_ALLOC_SIZE))
			{
				ret = -EINVAL;
				goto ioctl_return;
			}

			/*
			 * allocation method: 
			 * reserved memory is default alloc memory region, if any error occurs
			 * while allocating reserved memory, program will transfer to dynamic
			 * kernel memory resource alloc.
			 */
			if(rsv_alloc(param.size, &(param.paddr), inst) != 0)
			{
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
				memalloc_debug("alloc reserved memory error\n");

				vaddr = (void *)kmalloc(param.size, GFP_KERNEL);
				if(vaddr == NULL)
				{
					memalloc_error("kmalloc for buffer failed!\n");
					ret = -ENOMEM;
					goto ioctl_return;
				}

				param.paddr = (unsigned int)virt_to_phys((unsigned long *)vaddr);
				/*
				 * each instance has a memory allocation list, it's max management number is 256
				 * each time alloc ioctl called, file private data will update
				 */
				m_insert_mem(param.paddr, inst);
#else	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
				memalloc_error("alloc memory error\n");

				ret = -ENOMEM;
				goto ioctl_return;
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
			}

			(inst->alloc_count)++;

			if(copy_to_user((memalloc_param_t *)arg, &param, sizeof(memalloc_param_t)))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			break;

		case MEMALLOC_FREE_BUF:
			if(inst->alloc_count == 0)
			{
				memalloc_error("nothing to be free\n");
				ret = -ENOMEM;
				goto ioctl_return;
			}

			if(copy_from_user(&param, (memalloc_param_t *)arg, sizeof(memalloc_param_t)))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			/* 
			 * first find whether physical address correspond memory region is in 
			 * reserved memory or not. if yes correspond memory region will be freed.
			 * if memory region is not allocate by reserved memory, I will call kfree
			 * to free memory region.
			 */
			if(rsv_free(param.paddr, inst) != 0)
			{
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
				memalloc_debug("rsv_free() error \n");
				if(m_free_mem(param.paddr, inst))
				{
					ret = -EFAULT;
					goto ioctl_return;
				}
#else	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
				memalloc_error("free memory error\n");

				ret = -EFAULT;
				goto ioctl_return;
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
			}

			param.size = 0;
			inst->alloc_count--;

			if(copy_to_user((memalloc_param_t *)arg, &param, sizeof(memalloc_param_t)))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			break;

		case MEMALLOC_SHM_GET:
			/*
			 * share memory is global to all processes, so do be careful to use it and 
			 * correctly free it at end of program process.
			 */
			if(m_global.sh_count == MEMALLOC_MAX_ALLOC)
			{
				memalloc_error("shared memory limits reaches\n");
				ret = -ENOMEM;
				goto ioctl_return;
			}

			memset(&shm, 0x00, sizeof(shm_param_t));

			if(copy_from_user(&shm, (shm_param_t *)arg, sizeof(shm_param_t)))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			/* find shared memory asigned by key whether exists */
			for(i = 0; i < MEMALLOC_MAX_ALLOC; i++)
			{
				if(shm.key == m_global.shm[i].key)
				{
					shm.shaddr = m_global.shm[i].shaddr;
					if(shm.size > m_global.shm[i].size)
					{
						memalloc_error("Asigned memory size not large enough\n");
						ret = -ENOMEM;
						goto ioctl_return;
					}

					(m_global.shm[i].dep_count)++;

					/* fill instance structure */
					for(j = 0; j < MEMALLOC_MAX_ALLOC; j++)
					{
						if(inst->shaddr[j] == 0)
						{
							inst->shaddr[j] = shm.shaddr;
							break;
						}
					}

					break;
				}

				if(i == MEMALLOC_MAX_ALLOC - 1)
				{
					vaddr = (void *)kmalloc(shm.size, GFP_KERNEL);
					if(vaddr == NULL)
					{
						memalloc_error("kmalloc for buffer failed!\n");
						ret = -ENOMEM;
						goto ioctl_return;
					}
					memset(vaddr, 0x00, shm.size);

					shm.shaddr = (unsigned int)virt_to_phys((unsigned long *)vaddr);

					/* fill global structure */
					for(j = 0; j < MEMALLOC_MAX_ALLOC; j++)
					{
						if(m_global.shm[j].shaddr == 0)
						{
							m_global.shm[j].shaddr		= shm.shaddr;
							m_global.shm[j].dep_count	= 1;
							m_global.shm[j].size		= shm.size;
							m_global.shm[j].key		= shm.key;
							break;
						}
					}

					/* fill instance structure */
					for(j = 0; j < MEMALLOC_MAX_ALLOC; j++)
					{
						if(inst->shaddr[j] == 0)
						{
							inst->shaddr[j] = shm.shaddr;
							break;
						}
					}
				}
			}

			if(copy_to_user((shm_param_t *)arg, &shm, sizeof(shm_param_t)))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			break;

		case MEMALLOC_SHM_FREE:
			if(copy_from_user(&key, (unsigned int *)arg, 4))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			for(i = 0; i < MEMALLOC_MAX_ALLOC; i++)
			{
				if(key == m_global.shm[i].key)
					break;

				if(i == MEMALLOC_MAX_ALLOC - 1)
				{
					memalloc_error("Error free share memory\n");
					ret = -EFAULT;
					goto ioctl_return;
				}
			}

			if(m_global.shm[i].dep_count > 0)
				m_global.shm[i].dep_count--;

			/* clear instance structure */
			for(j = 0; j < MEMALLOC_MAX_ALLOC; j++)
			{
				if(inst->shaddr[j] == m_global.shm[i].shaddr)
				{
					inst->shaddr[j] = 0;
					break;
				}
			}

			if((m_global.shm[i].shaddr != 0) && (m_global.shm[i].dep_count == 0))
			{
				kfree((unsigned int *)phys_to_virt(m_global.shm[i].shaddr));
				/* clear global structure */
				memset(&(m_global.shm[i]), 0x00, sizeof(shm_param_t));
			}

			break;

		case MEMALLOC_GET_MPDMA_REG:
			__put_user(MPDMA_BASE_REG_PA, (unsigned int *)arg);
			break;

		case MEMALLOC_FLUSH_RSV:
			/* 
			 * WARNING: DON'T CALL UNLESS YOU ARE SURE
			 * this ioctl command will flush all reserved memory, 
			 * don't call this if you are not sure what will happen.
			 */
			{
				rsv_mem_t *tmp_cur = NULL;
				rsv_mem_t *tmp_pre = NULL;

				if(rsv_head != NULL)
				{
					/* head node's pre node pointer is supposed to be NULL */
					tmp_cur = tmp_pre = rsv_head->next;

					while(1)
					{
						/* reach the end of reserved memory list */
						if(tmp_cur == NULL)
							break;

						tmp_cur = rsv_head->next;
						kfree(tmp_pre);
						tmp_pre = tmp_cur;
					}

					/* physical and virtual address is not changed */
					rsv_head->mark = 0;
					rsv_head->size = rsv_size;
					rsv_head->pre = NULL;
					rsv_head->next = NULL;
				}
			}

			break;

		/*
		 * following fourfourfourfour ioctl commands are used by mm_dma library function 
		 * MmDMACopy(). this ioctl only called when one of copy address is user
		 * space address.
		 */
		case MEMALLOC_MPDMA_COPY:
#if 0
			/* 
			 * if following code error occurs, mpdma_mark must be set idle, this 
			 * mark is not so clever designed, cuz we don't need it to poll till
			 * a usable status.
			 */
			if(mpdma_mark == MPDMA_IDLE)
				mpdma_mark = MPDMA_USED;
			else
			{
				ret = EAGAIN;
				break;
			}

			if(copy_from_user(&mpdma, (mpdma_param_t *)arg, sizeof(mpdma_param_t)))
			{
				ret = -EFAULT;
				mpdma_mark = MPDMA_IDLE;
				goto ioctl_return;
			}

			ret = mpdma_memcpy(&mpdma);
			if(ret < 0)
			{
				mpdma_mark = MPDMA_IDLE;
				goto ioctl_return;
			}

			mpdma_mark = MPDMA_IDLE;
#else
			if(copy_from_user(&mpdma, (mpdma_param_t *)arg, sizeof(mpdma_param_t)))
			{
				ret = -EFAULT;
				goto ioctl_return;
			}

			ret = mpdma_memcpy(&mpdma);
			if(ret < 0)
				goto ioctl_return;
#endif

			break;

#if 0
		case MEMALLOC_GET_MPDMA_MARK:
			if(mpmark == NULL)
			{
				memalloc_error("mpdma mark error\n");
				ret = -EFAULT;
				goto ioctl_return;
			}
			else 
				__put_user((unsigned int)virt_to_phys(mpmark), (unsigned int *)arg);

			break;

		case MEMALLOC_LOCK_MPDMA:
			if(mpdma_mark == MPDMA_IDLE)
				mpdma_mark = MPDMA_USED;
			else 
			{
				memalloc_debug("memory pool is being used\n");
				ret = EAGAIN;
			}

			break;

		case MEMALLOC_UNLOCK_MPDMA:
			mpdma_mark = MPDMA_IDLE;
			break;
#endif

		default:
			memalloc_error("unknown ioctl command\n");
			ret = -EFAULT;
			goto ioctl_return;
	}

	memalloc_debug("Memalloc ioctl OK\n");

ioctl_return:
	mutex_unlock(m_mutex);
	return ret;
}

static int memalloc_mmap(struct file *file, struct vm_area_struct *vma)
{
	size_t size;

	mutex_lock(m_mutex);

	/* set memory map access to be uncached */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	size = vma->vm_end - vma->vm_start;
	if(remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot))
	{
		mutex_unlock(m_mutex);
		return -EAGAIN;
	}

	mutex_unlock(m_mutex);
	return 0;
}

static struct file_operations memalloc_fops = 
{
	.owner		= THIS_MODULE,
	.open		= memalloc_open,
	.release	= memalloc_release,
	.ioctl		= memalloc_ioctl,
	.mmap		= memalloc_mmap,
};

/*
 * register char device, alloc memory for mutex 
 * it's called by init function
 */
static int memalloc_probe(struct platform_device *pdev)
{
	/* initualize global variables */
	memset(&m_global, 0x00, sizeof(memalloc_global_t));

	/* 0 refers to dynamic alloc major number */
	m_global.major = register_chrdev(MEMALLOC_DEFAULT_MAJOR, "memalloc", &memalloc_fops);
	if(m_global.major < 0)
	{
		memalloc_error("Register char device for memalloc error\n");
		return -EFAULT;
	}

	m_global.major = MEMALLOC_DEFAULT_MAJOR;
	m_global.minor = MEMALLOC_DEFAULT_MINOR;

	m_class = class_create(THIS_MODULE, "memalloc");
	if(m_class == NULL)
	{
		memalloc_error("create char device driver class error\n");
		return -EFAULT;
	}

	device_create(m_class, NULL, \
			MKDEV(m_global.major, m_global.minor), \
			NULL, "memalloc");

	m_mutex = (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if(m_mutex == NULL)
	{
		memalloc_error("kmalloc for mutex error\n");
		return -ENOMEM;
	}
	mutex_init(m_mutex);

	/* map dma registers */
	mpdma_reg = ioremap_nocache(MPDMA_BASE_REG_PA, sizeof(mpdma_reg_t));
	if(mpdma_reg == NULL)
	{
		memalloc_error("ioremap_nocache() for memory pool dma register error\n");
		return -ENOMEM;
	}

	/* enable memory pool */
	if(m_enable_mp())
	{
		memalloc_error("fail to enable memory pool\n");
		return -EFAULT;
	}

/* RESERVED MEMORY */

	/*
	 * initialize reserved memory list header, at start of system boot, there is
	 * just only one big memory region. this region is the size of reserved memory,
	 * and start address is the reserved memory, any following allocation will devide
	 * this origin block into many small regions. I only use one list to hold all 
	 * memory block, so all connected memory regions is address linear. it will be 
	 * very easy to merge consecutive idle block.
	 */
	/* alloc structure for first block */
	rsv_head = (rsv_mem_t *)kmalloc(sizeof(rsv_mem_t), GFP_KERNEL);
	if(rsv_head == NULL)
	{
		memalloc_error("alloc structure memory for reserved list head error\n");
		return -ENOMEM;
	}
	memset(rsv_head, 0x00, sizeof(rsv_mem_t));

	rsv_head->mark	= 0;	/* supposed to be idle block */
	rsv_head->pre	= NULL;
	rsv_head->next	= NULL;
	/*
	 * reserved memory start address is supposed to page size aligned. reserved 
	 * memory is set in kernel start boot config, best size and start address 
	 * should be mutiple of 4KB(page size), cuz if allocated buffer address is 
	 * not page size aligned, mmap might fail(must fail in Android bionic lib).
	 */
#if defined(CONFIG_IMAP_MEMALLOC_MANUAL_RESERVE)
	rsv_phys	= MEMALLOC_RSV_ADDR;
	rsv_size	= MEMALLOC_RSV_SIZE;
	rsv_phys_end	= rsv_phys + rsv_size;

	/* 
	 * FIXME: maybe it's not neccessary to map reserved memory to kernel memory space 
	 * whole memory region is map to be nocache, cuz is for decode and encode, display
	 * use, hardware use physicall address only.
	 */
	rsv_virt = (unsigned int *)ioremap_nocache(rsv_phys, rsv_size);
	if(rsv_virt == NULL)
	{
		memalloc_error("remap reserved memory error\n");
		return -ENOMEM;
	}

	rsv_head->phys	= rsv_phys;
	rsv_head->size	= rsv_size;
	rsv_head->virt	= rsv_virt;
#elif defined(CONFIG_IMAP_MEMALLOC_SYSTEM_RESERVE)
	rsv_phys	= (unsigned int)imap_get_reservemem_paddr(RESERVEMEM_DEV_MEMALLOC);
	rsv_size	= (unsigned int)imap_get_reservemem_size(RESERVEMEM_DEV_MEMALLOC);
	rsv_phys_end	= rsv_phys + rsv_size;
	if((rsv_phys == 0) || (rsv_size == 0))
	{
		memalloc_error("memalloc reserved memory physical address or size is invalid\n");
		return -EFAULT;
	}

	rsv_virt = (unsigned int *)ioremap_nocache(rsv_phys, rsv_size);
	if(rsv_virt == NULL)
	{
		memalloc_error("remap reserved memory error\n");
		return -ENOMEM;
	}

	rsv_head->phys	= rsv_phys;
	rsv_head->virt	= rsv_virt;
	rsv_head->size	= rsv_size;
#else
#error Unknow memalloc reserved memory type!
#endif

/* PROCESS EXPORT RELATED VARIABLES */

	memset(&export_inst, 0x00, sizeof(memalloc_inst_t));

	export_mark = 1;	/* mark export api usable */

#if 0
	/* alloc mpdma mark */
	mpmark = (unsigned int *)kmalloc(4 * 1024, GFP_KERNEL);
	if(mpmark == NULL)
	{
		memalloc_error("alloc memory for memory pool mark error\n");
		return -ENOMEM;
	}
	memset(mpmark, 0x00, 4 * 1024);
#endif

	return 0;
}

static int memalloc_remove(struct platform_device *pdev)
{
	int i;

	for(i = 0; i < MEMALLOC_MAX_ALLOC; i++)
	{
		if(m_global.shm[i].shaddr != 0)
		{
			kfree((unsigned long *)phys_to_virt(m_global.shm[i].shaddr));
			memset(&(m_global.shm[i]), 0x00, sizeof(shm_param_t));
		}
	}

	m_disable_mp();
	if(mpdma_reg != NULL)
		iounmap(mpdma_reg);

/* RESERVED MEMORY */

	/*
	 * recycle system resources, delete reserved memory list, and free all
	 * allocated memory.
	 */
	{
		rsv_mem_t *tmp_cur = NULL;
		rsv_mem_t *tmp_pre = NULL;

		if(rsv_head != NULL)
		{
			/* head node's pre node pointer is supposed to be NULL */
			tmp_cur = tmp_pre = rsv_head->next;

			while(1)
			{
				/* reach the end of reserved memory list */
				if(tmp_cur == NULL)
					break;

				tmp_cur = tmp_cur->next;
				kfree(tmp_pre);
				tmp_pre = tmp_cur;
			}

			if(rsv_head->virt != NULL)
				iounmap((void *)(rsv_head->virt));
			kfree(rsv_head);
		}

		rsv_phys	= 0;
		rsv_size	= 0;
		rsv_virt	= NULL;
	}

	export_mark = 0;

#if 0
	if(mpmark != NULL)
		kfree(mpmark);
#endif

	device_destroy(m_class, MKDEV(m_global.major, m_global.minor));
	class_destroy(m_class);
	unregister_chrdev(m_global.major, "memalloc");

	return 0;
}

static int memalloc_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* just disable memory pool */
	m_disable_mp();

	return 0;
}

static int memalloc_resume(struct platform_device *pdev)
{
	/* enable memory pool */
	m_enable_mp();

	return 0;
}

/*
 * platform driver structure
 */
static struct platform_driver memalloc_driver = 
{
	.probe		= memalloc_probe,
	.remove		= memalloc_remove,
	.suspend	= memalloc_suspend,
	.resume		= memalloc_resume,
	.driver	=
	{
		.owner	= THIS_MODULE,
		.name	= "memalloc",
	},
};

/*
 * this memalloc node is a char driver but probe as platform device
 */
static int __init memalloc_init(void)
{
	if(platform_driver_register(&memalloc_driver))
	{
		memalloc_error("Register platform device error\n");
		return -EPERM;
	}

	memalloc_debug("Memalloc initualize OK\n");

	return 0;
}

static void __exit memalloc_exit(void)
{
	platform_driver_unregister(&memalloc_driver);
	mutex_destroy(m_mutex);
	kfree(m_mutex);

	memalloc_debug("Memalloc exit OK\n");
}

module_init(memalloc_init);
module_exit(memalloc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sololz of InfoTM");
MODULE_DESCRIPTION("Memory tool for Decode and Encode mainly");

/*
 * system call no related functions
 */

/*  
 * m_reset_mem()
 * this function free all memory allocated in current instance
 */
int m_reset_mem(memalloc_inst_t *inst)
{
	int i;

	for(i = 0; i < MEMALLOC_MAX_ALLOC; i++)
	{
		if(inst->paddr[i] != 0)
		{
			memalloc_debug("memory alloced by /dev/memalloc didn't free normally, fix here\n");
			/* check whether this memory block is in reserved memory */
			if(rsv_free(inst->paddr[i], inst) != 0)
			{
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
				/* FIXME: this code based on reserved memory is set at end of physical memory */
				if((inst->paddr[i] < rsv_phys) || (inst->paddr[i] > rsv_phys_end))
					kfree((unsigned int *)phys_to_virt(inst->paddr[i]));
#else	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
				memalloc_debug("memory block is in instance structure, but not in reserved memory\n");
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
			}

			inst->paddr[i] = 0;
		}
	}

	return 0;
}

/*
 * m_insert_mem()
 * this function insert a new allocated memory block in to instance record
 */
int m_insert_mem(unsigned int paddr, memalloc_inst_t *inst)
{
	int i;

	for(i = 0; i < MEMALLOC_MAX_ALLOC; i++)
	{
		if(inst->paddr[i] == 0)
		{
			inst->paddr[i] = paddr;
			memalloc_debug("memory block get inserted\n");
			break;
		}
	}

	return 0;
}

#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
/*
 * m_free_mem()
 * this function free one memory block by its address
 * I have to admit that it's not a good algorithm to locate
 */
int m_free_mem(unsigned int paddr, memalloc_inst_t *inst)
{
	int i;

	for(i = 0; i < MEMALLOC_MAX_ALLOC; i++)
	{
		if(inst->paddr[i] == paddr)
		{
			memalloc_debug("memory block located to free\n");
			if(paddr < rsv_phys)
				kfree((unsigned long *)phys_to_virt(paddr));

			inst->paddr[i] = 0;

			break;
		}

		if(i == MEMALLOC_MAX_ALLOC - 1)
		{
			memalloc_error("no such memory block\n");
			return -ENOMEM;
		}
	}

	return 0;
}
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@    MEMORY POOL HARDWARE INIT AND DEINIT */

/*
 * m_enable_mp()
 * enable memory poll by control manager registers
 */
int m_enable_mp(void)
{
	int i;
	volatile unsigned int val;
	mpdma_reg_t *reg;

	/* set mempool power */
	val = readl(rNPOW_CFG);
	val |= (1 << 2);
	writel(val, rNPOW_CFG);
	while(!(readl(rPOW_ST) & (1 << 2)));

	/* reset mempool power */
	val = readl(rMD_RST);
	val |= (1 << 2);
	writel(val, rMD_RST);
	for(i = 0; i < 0x1000; i++);

	/* isolate mempool */
	val = readl(rMD_ISO);
	val &= ~(1 << 2);
	writel(val, rMD_ISO);
	val = readl(rMD_RST);
	val &= ~(1 << 2);
	writel(val, rMD_RST);

	/* reset mempool */
	val = readl(rAHBP_RST);
	val |= (1 << 22);
	writel(val, rAHBP_RST);
	for(i = 0; i < 0x1000; i++);
	val = readl(rAHBP_RST);
	val &= ~(1 << 22);
	writel(val, rAHBP_RST);

	/* enable mempool */
	val = readl(rAHBP_EN);
	val |= (1 << 22);
	writel(val, rAHBP_EN);

	/* set memory pool to decode mode */
	val = readl(rMP_ACCESS_CFG);
	val |= 1;
	writel(val, rMP_ACCESS_CFG);

	/* disable mpdma interrupt */
	if(mpdma_reg == NULL)
	{
		return -1;
	}
	reg = (mpdma_reg_t *)mpdma_reg;
	reg->int_en = 0;

	return 0;
}

/*
 * m_disable_mp()
 * disable memory poll by control magager registers
 */
int m_disable_mp(void)
{
	volatile unsigned int val;

	/* disable mempool */
	val = readl(rAHBP_EN);
	val &= ~(1 << 22);
	writel(val, rAHBP_EN);

	/* unisolate mempool */
	val = readl(rMD_ISO);
	val |= (1 << 2);
	writel(val, rMD_ISO);

	/* shut mempool power */
	val = readl(rNPOW_CFG);
	val &= ~(1 << 2);
	writel(val, rNPOW_CFG);
	while(readl(rPOW_ST) & (1 << 2));

	return 0;
}

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ RESERVED MEMORY FUNCTIONS */

/*
 * rsv_alloc()
 * HOWTO:
 * here just use one memory region list to preserve memory block in
 * reserved memory area, and alloc method is low address first, and 
 * connected memory regions must be address sequence. this design will
 * be quite easy to guarantee that after free reserved memory region,
 * connected idle reserved memory region will be merged. but the disadvantage
 * of this method is that, if most of the allocated memory size is quite
 * so small, it will take a quite long time to find an suitable idle 
 * reserved memory block. O(n) time cost should be improved in the 
 * future.
 * considering the reserved memory region is mainly used by media player
 * and display, most of the reserved memory is allocated at start of media
 * player start, and freed at media player ends. so I set a current node
 * mark to mark for lastest allocation block. and next allocation will 
 * check from this pointer node and save alot of alloc time. 
 * this function allocate memory region in reserved memory, if success, 
 * will return 0, else a negative interger. this function not only alloc
 * but also insert allocated memory region into instance structure.
 */
int rsv_alloc(unsigned int size, unsigned int *paddr, memalloc_inst_t *inst)
{
	rsv_mem_t *cur = NULL;
	rsv_mem_t *tmp = NULL;

	/* size and paddr, inst validation need no check */
	memalloc_debug("get into reserved memory alloc\n");

	/* 
	 * from current list next node start, find fits blank memory block, rsv_cur 
	 * is supposed to pointer to last allocate memory structure. normally,
	 * next node of rsv_cur should be a large blank memory region.
	 */
	cur = rsv_head;
	while(1)
	{
		if(cur == NULL)
		{
			memalloc_debug("can't find fit memory region in reserved memory\n");
			return -ENOMEM;
		}

		if((cur->mark == 0) && (cur->size >= size))	/* find suitable memory region */
		{
			if(cur->size == size)
			{
				cur->mark = 1;
				*paddr = cur->phys;
			}
			else
			{
				/* current block is larger than required size, so devide current region */
				tmp = (rsv_mem_t *)kmalloc(sizeof(rsv_mem_t), GFP_KERNEL);
				if(tmp == NULL)
				{
					memalloc_error("malloc for reserved memory node structure error\n");
					return -ENOMEM;
				}
				memset(tmp, 0x00, sizeof(rsv_mem_t));

				/* update list structure */
				if(cur->next != NULL)	/* reset third node */
					cur->next->pre = tmp;
				tmp->next = cur->next;
				tmp->pre = cur;
				cur->next = tmp;

				/* reset devided memory region size, and address */
				tmp->mark = 0;
				tmp->size = cur->size - size;
				tmp->phys = cur->phys + size;
				tmp->virt = (unsigned int *)((unsigned int)(cur->virt) + size);

				cur->mark = 1;
				cur->size = size;

				*paddr = cur->phys;
			}

			/* insert current memory block into instance structure */
			m_insert_mem(*paddr, inst);

#ifdef CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE
			trace_memory += size;

			memalloc_error("alloc address 0x%08x, total memory usage %d\n", *paddr, trace_memory);
#endif	/* CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE */

			break;
		}
		else
			cur = cur->next;
	}

	memalloc_debug("reserved memory alloc ok\n");
	return 0;
}

/*
 * rsv_free()
 * first find correspond physical address in reserved memory, if memory
 * region found, it will be deleted from instance structure, else, return
 * negative interger.
 */
int rsv_free(unsigned int paddr, memalloc_inst_t *inst)
{
	int i;
	rsv_mem_t *cur = NULL;
	rsv_mem_t *tmp = NULL;

	/* size and paddr, inst validation need no check */
	memalloc_debug("get into reserved memory free\n");

	/* 
	 * find correspond reserved memory region in reserved memory list, I have not
	 * found a better method to find the paddr in reserved memory list, currently, 
	 * just find node from head.
	 */
	cur = rsv_head;
	while(1)
	{
		if(cur == NULL)
		{
			memalloc_debug("no such reserved memory in reserved memory list\n");
			return -EINVAL;
		}

		if(cur->phys == paddr)
		{
			tmp = cur;	/* tmp records fit memory block node pointer */
			tmp->mark = 0;	/* set current memory block to be idle */

#ifdef CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE
			trace_memory -= cur->size;
			memalloc_alert("Total memory usage %d\n", trace_memory);
#endif	/* CONFIG_IMAP_MEMALLOC_MEMORY_USAGE_TRACE */

			if(tmp->pre != NULL)
			{
				if(tmp->pre->mark == 0)	/* merge block, save prenode */
				{
					cur = tmp->pre;

					cur->size = cur->size + tmp->size;
					cur->next = tmp->next;
					if(cur->next != NULL)
						cur->next->pre = cur;
				}
			}

			if(cur->next != NULL)
			{
				if(cur->next->mark == 0)
				{
					rsv_mem_t *trans = NULL;

					cur->size = cur->size + cur->next->size;
					trans = cur->next;
					cur->next = cur->next->next;
					if(cur->next != NULL)
						cur->next->pre = cur;

					if(trans != NULL)
						kfree(trans);
				}
			}

			if(cur != tmp)
				kfree(tmp);

			break;
		}

		cur = cur->next;
	}

	/* clean instance structure */
	for(i = 0; i < MEMALLOC_MAX_ALLOC; i++)
	{
		if(inst->paddr[i] == paddr)
		{
			memalloc_debug("memory block located to free\n");
			inst->paddr[i] = 0;
			break;
		}

		if(i == MEMALLOC_MAX_ALLOC - 1)
		{
			memalloc_error("no such memory block in instance structure\n");
			return -ENOMEM;
		}
	}

	memalloc_debug("reserved memory free ok\n");

	return 0;
}

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ EXPORT RESERVED MEMROY API */

/*
 * lpmem_alloc()
 * This function is exported for kernel use to allocate physical linear 
 * memory. Mainly designed for GPU use, also any other driver can call 
 * this function to allocate physical linear memory in standardization.
 * Allocate memory block size best be page align.
 */
int lpmem_alloc(lpmem_t *mem)
{
	unsigned int size = 0;
	unsigned int psize = 4 * 1024;

	/* check parameter */
	if(!export_mark)
	{
		memalloc_error("memory not prepared to be allocated\n");
		return -EFAULT;
	}
	else if(export_inst.alloc_count > MEMALLOC_MAX_ALLOC)
	{
		memalloc_error("no memory to alloc in export instance\n");
		return -ENOMEM;
	}
	else if(mem == NULL)
	{
		memalloc_error("input parameter error\n");
		return -EINVAL;
	}
	else if((mem->size <= 0) || (mem->size > MEMALLOC_MAX_ALLOC_SIZE))
	{
		memalloc_error("alloc size invalid, %d\n", mem->size);
		return -EINVAL;
	}

	size = (mem->size + psize - 1) & (~(psize - 1));

	mutex_lock(m_mutex);

	if(rsv_alloc(size, &(mem->phys), &export_inst) != 0)
	{
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
		mem->virt = (unsigned int *)kmalloc(size, GFP_KERNEL);
		if(mem->virt == NULL)
		{
			memalloc_error("kernel export alloc function error\n");
			mutex_unlock(m_mutex);
			return -ENOMEM;
		}
		mem->phys = (unsigned int)virt_to_phys(mem->virt);

		m_insert_mem(mem->phys, &export_inst);
#else	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
		memalloc_error("kernel export alloc function error\n");
		mutex_unlock(m_mutex);
		return -ENOMEM;
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
	}

	/* set alloc memory kernel space virtual address */
	if((mem->phys >= rsv_phys) && (mem->phys < rsv_phys_end))
		mem->virt = (unsigned int *)((unsigned int)(rsv_virt) + (mem->phys - rsv_phys));

	export_inst.alloc_count++;

	mutex_unlock(m_mutex);

	return 0;
}
EXPORT_SYMBOL(lpmem_alloc);

/*
 * lpmem_free()
 * This function is exported for kernel use to free physical linear
 * memory allocated by lpmem_alloc().
 */
int lpmem_free(lpmem_t *mem)
{
	/* check parameter */
	if(!export_mark)
	{
		memalloc_error("memory not prepared to be freed\n");
		return -EFAULT;
	}
	else if(export_inst.alloc_count <= 0)
	{
		memalloc_error("nothing to be freed in instance\n");
		return -EFAULT;
	}
	else if(mem == NULL)
	{
		memalloc_error("input parameter error\n");
		return -EINVAL;
	}

	mutex_lock(m_mutex);

	if(rsv_free(mem->phys, &export_inst) != 0)
	{
#ifdef CONFIG_IMAP_MEMALLOC_USE_KMALLOC
		if(m_free_mem(mem->phys, &export_inst))
		{
			memalloc_error("no such memory in export instance to be freed\n");
			mutex_unlock(m_mutex);
			return -EFAULT;
		}
#else	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
		memalloc_error("no such memory in export instance to be freed\n");
		mutex_unlock(m_mutex);
		return -EFAULT;
#endif	/* CONFIG_IMAP_MEMALLOC_USE_KMALLOC */
	}

	if(export_inst.alloc_count > 0)
		export_inst.alloc_count--;

	mutex_unlock(m_mutex);

	return 0;
}
EXPORT_SYMBOL(lpmem_free);

/* @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ MEMORY POOL COPY CODE PART */

/*
 * mpdma_memcpy()
 * Copy user space address memory using memory pool. Copy method must be 
 * done in kernel space, because of safety consideration and user space 
 * has no function to get virtual address correspond physical address.
 * And hardware need physical address. This function also support physical
 * address to physical address copy, but it's export for driver use.
 */
int mpdma_memcpy(mpdma_param_t *mpdma)
{
	unsigned int	size;
	unsigned int	*vsrc;
	unsigned int	psrc;
	unsigned int	*vdst;
	unsigned int	pdst;
	unsigned int	ref	= 0;	/* refer to source address by default, 0 - src, 1 - dst */
	mpdma_reg_t	*mpreg	= (mpdma_reg_t *)mpdma_reg;

/* check parameters */

	if(mpreg == NULL)
	{
		memalloc_error("mpdma hardware not prepared\n");
		return -EINVAL;
	}

	if(mpdma == NULL)
	{
		memalloc_error("input parameter error\n");
		return -EINVAL;
	}
	size	= mpdma->size;
	vsrc	= mpdma->vsrc;
	psrc	= mpdma->psrc;
	vdst	= mpdma->vdst;
	pdst	= mpdma->pdst;
	if((size <= 0) || (size > MEMALLOC_MAX_ALLOC_SIZE))
	{
		memalloc_error("copy size is invalid, %d\n", size);
		return -EINVAL;
	}
	else if((vsrc == NULL) && (psrc <= IMAPX200_MEMORY_START))
	{
		memalloc_error("source memory address error\n");
		return -EINVAL;
	}
	else if((vdst == NULL) && (pdst <= IMAPX200_MEMORY_START))
	{
		memalloc_error("dest memory address error\n");
		return -EINVAL;
	}
	else 
	{
		/* everything seems to be ok */
	}

	/* check source and dest address to decide refer type */
	if((psrc != 0) && (vdst != NULL) && (pdst == 0))
		ref = 1;
	else 
		ref = 0;

	memalloc_debug("[MEMALLOC] ref = %d\n", ref);

/* start copy operation */

	mpreg->ch0_maddr = 0x30000;
	mpreg->ch1_maddr = 0x30000;

	/*
	 * copy size is set to be 4KB each time, if start address is not 4KB,
	 * first unaligned memory region will be tranferred. though both src
	 * and dst is physical address situation can be handled here, but not
	 * faster than user space code. memory copy situation gets here suposed
	 * to be that one of src and dst is virtual memory buffer. if either src
	 * and dst address are not multiple of 4KB, and srcaddr % 4KB doesn't
	 * equal dstaddr % 4KB, this copy method will get worst performance.
	 * cuz all memory copy is less than 4KB means that alway need 2 time
	 * copy operation to finish 4KB size transfer. size align refers to 
	 * source address by default.
	 */
	while(size > 0)
	{
		unsigned int	saddr	= 0;		/* source physical address set to dma hardware */
		unsigned int	daddr_0	= 0;		/* dest physical address set to dma hardware */
		unsigned int	daddr_1	= 0;		/* dest physical address set to dma hardware */
		unsigned int	cplen	= 0;		/* copy length in current loop */
		unsigned int	pgsize	= 1024 * 4;	/* arm platform page size is supposed to 4KB */
		unsigned int	pgnum	= 0;		/* return value of get_user_pages(), represents number of get pages */
		struct page	*spage	= NULL;
		struct page	*dpage_0 = NULL;
		struct page	*dpage_1 = NULL;
		unsigned int	cplen_0	= 0;		/* dest address might be devide to 2 part to tranfer */
		unsigned int	cplen_1	= 0;
		unsigned int	*line_addr = NULL;

		/* get source memory address copy size */
		if(ref == 0)
		{
			if(psrc != 0)
				cplen = pgsize - (psrc & (pgsize - 1));
			else
				cplen = pgsize - (((unsigned int)vsrc) & (pgsize - 1));
		}
		else 
		{
			if(pdst != 0)
				cplen = pgsize - (pdst & (pgsize - 1));
			else
				cplen = pgsize - (((unsigned int)vdst) & (pgsize - 1));
		}

		memalloc_debug("[MEMALLOC] copy len %d\n", cplen);

/* do copy */

		if(size <= cplen)
			cplen = size;

		/* get source physical address */
		if(psrc != 0)
			saddr = psrc;
		else
		{
			/*
			 * source virtual address doesn't need 2 address variable 
			 * to do depart address convertion and data transferring.
			 * because if source virtual address is used to do tranfer,
			 * source is the reference size. get_user_pages() function
			 * must be used with mmap_sem held, both read and write
			 * held will be ok, considering kernel code all use read
			 * held, so down_read() is used here.
			 */
			down_read(&(current->mm->mmap_sem));
			pgnum = get_user_pages(current, current->mm, (unsigned int)vsrc & (~(pgsize - 1)), \
					1, 0/* source page not suppose to be written */, 0, &(spage), NULL);
			up_read(&(current->mm->mmap_sem));
			if((pgnum != 1) || (spage == NULL))
			{
				memalloc_error("get_user_pages() error\n");
				spage = NULL;
				goto mpdma_copy_error;
			}

			/*
			 * FIXME TODO
			 * here i use page_address to get kernel linear virtual address instead of 
			 * kmap cuz kmap is just a function pack of page_address. if page correspond
			 * physical address has not been mapped to kernel linear logical address 
			 * space or high memory virtual address space, kmap will map this page to 
			 * high memory. but i don't know how to process highmemory address to convert
			 * to physical address. so all pages is supposed that have been mapped to 
			 * kernel logical address. according to "understanding linux kernel" book, 
			 * high memory address start at (3GB + 896MB) place, so if platform memory
			 * is less than 896MB, following code will always work. else some errors will
			 * happen. so any one know how to convert highmemory address(not by vmalloc)
			 * to physical address, please let me know.
			 */
			line_addr = (unsigned int *)page_address(spage);
			if(line_addr == NULL)
			{
				memalloc_error("page address is NULL\n");
				goto mpdma_copy_error;
			}
			line_addr = (unsigned int *)((unsigned int)line_addr + ((unsigned int)vsrc & (pgsize - 1)));

			saddr = dma_map_single(NULL, (void *)line_addr, cplen, DMA_TO_DEVICE);
			if(saddr < IMAPX200_MEMORY_START)
			{
				memalloc_error("get dma bus address error\n");
				goto mpdma_copy_error;
			}
		}

		/* get dest physical address */
		if(pdst != 0)
		{
			daddr_0 = pdst;
			cplen_0 = cplen;
			cplen_1 = 0;
		}
		else
		{
			unsigned int dst_redu = pgsize - ((unsigned int)vdst & (pgsize - 1));

			/*
			 * dest virtual address redundance size less than source virtual address
			 * redundance size, in this case, 2 part of dest to transfer all data.
			 */
			if((ref == 0) && (dst_redu < cplen))	/* source refer */
			{
#if 0	/* this case should never happen, precheck will ensure that */
				if(vsrc == NULL)
				{
					memalloc_error("logic error\n");
					goto mpdma_copy_error;
				}
#endif
				/* get first part page */
				down_read(&(current->mm->mmap_sem));
				pgnum = get_user_pages(current, current->mm, (unsigned int)vdst & (~(pgsize - 1)), \
						1, 1, 0, &dpage_0, NULL);
				up_read(&(current->mm->mmap_sem));
				if((pgnum != 1) || (dpage_0 == NULL))
				{
					memalloc_error("get_user_pages() error\n");
					dpage_0 = NULL;
					goto mpdma_copy_error;
				}

				line_addr = (unsigned int *)page_address(dpage_0);
				if(line_addr == NULL)
				{
					memalloc_error("page address is NULL\n");
					goto mpdma_copy_error;
				}
				line_addr = (unsigned int *)((unsigned int)line_addr + ((unsigned int)vdst & (pgsize - 1)));

				cplen_0 = dst_redu;
				cplen_1 = cplen - cplen_0;

				daddr_0 = dma_map_single(NULL, (void *)line_addr, cplen_0, DMA_FROM_DEVICE);
				if(daddr_0 < IMAPX200_MEMORY_START)
				{
					memalloc_error("get dma bus address error\n");
					goto mpdma_copy_error;
				}

				/* get second part page */
				down_read(&(current->mm->mmap_sem));
				pgnum = get_user_pages(current, current->mm, (unsigned int)vdst + cplen_0, \
						1, 1, 0, &dpage_1, NULL);
				up_read(&(current->mm->mmap_sem));
				if((pgnum != 1) || (dpage_1 == NULL))
				{
					memalloc_error("get_user_pages() error\n");
					dpage_1 = NULL;
					goto mpdma_copy_error;
				}
				line_addr = (unsigned int *)page_address(dpage_1);

				daddr_1 = dma_map_single(NULL, (void *)line_addr, cplen_1, DMA_FROM_DEVICE);
				if(daddr_1 < IMAPX200_MEMORY_START)
				{
					memalloc_error("get dma bus address error\n");
					goto mpdma_copy_error;
				}
			}
			else
			{
				/* 
				 * in this case, dest virtual address is reference address. so only
				 * one part of page is used to do data transfer.
				 */
				down_write(&(current->mm->mmap_sem));
				pgnum = get_user_pages(current, current->mm, (unsigned int)vdst & (~(pgsize - 1)), \
						1, 1, 0, &(dpage_0), NULL);
				up_write(&(current->mm->mmap_sem));
				if((pgnum != 1) || (dpage_0 == NULL))
				{
					memalloc_error("get_user_pages() error, %d\n", pgnum);
					dpage_0 = NULL;
					goto mpdma_copy_error;
				}

				line_addr = (unsigned int *)page_address(dpage_0);
				if(line_addr == NULL)
				{
					memalloc_error("page_address() return NULL\n");
					goto mpdma_copy_error;
				}
				line_addr = (unsigned int *)((unsigned int)line_addr + ((unsigned int)vdst & (pgsize - 1)));

				daddr_0 = dma_map_single(NULL, (void *)line_addr, cplen, DMA_FROM_DEVICE);
				if(daddr_0 < IMAPX200_MEMORY_START)
				{
					memalloc_error("get dest dma bus address error\n");
					goto mpdma_copy_error;
				}

				cplen_0 = cplen;
				cplen_1 = 0;
			}
		}

/* start dma copy */

		/* use memory pool to move part 1 data */
		/* first part must be transfered, cuz it's a must existance */
		mpreg->ch0_saddr = saddr;
		mpreg->ch1_saddr = daddr_0;

		mpreg->dma_en = 0;
		mpreg->dma_en = 1;

		mpreg->ch1_ctrl = cplen_0 | 0x02000000;
		mpreg->ch0_ctrl = cplen_0 | 0x8c000000;

		while(mpreg->ch0_ctrl & 0x80000000);

		/* use memory pool to move part 2 data */
		/* second part might not be existed */
		if(cplen_1 > 0)
		{
			mpreg->ch0_saddr = saddr + cplen_0;
			mpreg->ch1_saddr = daddr_1;

			mpreg->dma_en = 0;
			mpreg->dma_en = 1;

			mpreg->ch1_ctrl = cplen_1 | 0x02000000;
			mpreg->ch0_ctrl = cplen_1 | 0x8c000000;

			while(mpreg->ch0_ctrl & 0x80000000);
		}

/* release page each time */

		if(spage != NULL)
		{
			SetPageDirty(spage);
			page_cache_release(spage);
			spage = NULL;
		}

		if(dpage_0 != NULL)
		{
			SetPageDirty(dpage_0);
			page_cache_release(dpage_0);
			dpage_0 = NULL;
		}

		if(dpage_1 != NULL)
		{
			SetPageDirty(dpage_1);
			page_cache_release(dpage_1);
			dpage_1 = NULL;
		}

/* process length */

		if(psrc != 0)
			psrc += cplen;
		else
			vsrc = (unsigned int *)((unsigned int)vsrc + cplen);

		if(pdst != 0)
			pdst += cplen;
		else
			vdst = (unsigned int *)((unsigned int)vdst + cplen);

		size -= cplen;

		if(size <= 0)
			break;

		continue;

mpdma_copy_error:
		if(spage != NULL)
		{
			SetPageDirty(spage);
			page_cache_release(spage);
			spage = NULL;
		}

		if(dpage_0 != NULL)
		{
			SetPageDirty(dpage_0);
			page_cache_release(dpage_0);
			dpage_0 = NULL;
		}

		if(dpage_1 != NULL)
		{
			SetPageDirty(dpage_1);
			page_cache_release(dpage_1);
			dpage_1 = NULL;
		}

		return -EFAULT;
	}

	return 0;
}
