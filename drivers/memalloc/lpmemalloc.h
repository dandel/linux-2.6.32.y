/*
 * lpmemalloc.h
 *
 * This head file exports functions in memalloc.c for other drivers 
 * to allocate physical linear memory. First this is designed for
 * GPU use, and caller must include this head file to call correspond
 * allocate and free functions.
 *
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Sololz <sololz.luo@gmail.com>
 *
 * 2010/07/21	create this file to export memory allocation function
 * 		for other drivers use.
 */

#ifndef __LPMEMALLOC_H__
#define __LPMEMALLOC_H__

/* this structure is for allocation parameter */
typedef struct
{
	unsigned int phys;	/* physical address */
	unsigned int *virt;	/* virtual address in kernel space use */
	unsigned int size;
}lpmem_t;

/*
 * function designed for kernel driver use, never export it to user space
 * application. if allocate success, 0 will be returned, else a negative
 * number will return.
 */
int lpmem_alloc(lpmem_t *mem);
int lpmem_free(lpmem_t *mem);

#endif	/* __LPMEMALLOC_H__ */
