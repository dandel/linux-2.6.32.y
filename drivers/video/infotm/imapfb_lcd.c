/***************************************************************************** 
** drivers/video/infotm/imapfb_lcd.c
** 
** Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
** 
** Use of Infotm's code is governed by terms and conditions 
** stated in the accompanying licensing statement. 
** 
** Description: Implementation file of Display Controller.
**
** Author:
**     Feng Jiaxing <jiaxing_feng@infotm.com>
**      
** Revision History: 
** ----------------- 
** 1.0  09/14/2009  Feng Jiaxing
*****************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/wait.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "imapfb.h"
#include "logo_new.h"
#include "logo.h"

#if defined(CONFIG_FB_IMAP_LCD800X480)
#define IMAPFB_HFP		15//8		/* front porch */
#define IMAPFB_HSW		104//3		/* hsync width */
#define IMAPFB_HBP		128//13		/* back porch */

#define IMAPFB_VFP		5//5		/* front porch */
#define IMAPFB_VSW		4//1		/* vsync width */
#define IMAPFB_VBP		21//7		/* back porch */

#define IMAPFB_HRES		800		/* horizon pixel x resolition */
#define IMAPFB_VRES		480		/* line cnt y resolution */

#define IMAPFB_HRES_OSD	800		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD	480		/* line cnt y resolution */

#define IMAPFB_HRES_OSD_VIRTUAL	800		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD_VIRTUAL	480		/* line cnt y resolution */

#define IMAPFB_VFRAME_FREQ	30		/* frame rate freq */
#elif defined(CONFIG_FB_IMAP_LCD640X480)
#define IMAPFB_HFP		16		/* front porch */
#define IMAPFB_HSW		96		/* hsync width */
#define IMAPFB_HBP		48		/* back porch */

#define IMAPFB_VFP		5//10		/* front porch */
#define IMAPFB_VSW		2//5		/* vsync width */
#define IMAPFB_VBP		13//34		/* back porch */

#define IMAPFB_HRES		640		/* horizon pixel x resolition */
#define IMAPFB_VRES		480		/* line cnt y resolution */

#define IMAPFB_HRES_OSD	640		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD	480		/* line cnt y resolution */

#define IMAPFB_HRES_OSD_VIRTUAL	640		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD_VIRTUAL	480		/* line cnt y resolution */

#define IMAPFB_VFRAME_FREQ	30		/* frame rate freq */
#elif defined(CONFIG_FB_IMAP_LCD1024X600)
#define IMAPFB_HFP		15		/* front porch */
#define IMAPFB_HSW		104		/* hsync width */
#define IMAPFB_HBP		128		/* back porch */

#define IMAPFB_VFP		69		/* front porch */
#define IMAPFB_VSW		4		/* vsync width */
#define IMAPFB_VBP		41		/* back porch */

#define IMAPFB_HRES		1024		/* horizon pixel x resolition */
#define IMAPFB_VRES		600		/* line cnt y resolution */

#define IMAPFB_HRES_OSD	1024		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD	600		/* line cnt y resolution */

#define IMAPFB_HRES_OSD_VIRTUAL	1024		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD_VIRTUAL	600		/* line cnt y resolution */

#define IMAPFB_VFRAME_FREQ	60		/* frame rate freq */
#elif defined(CONFIG_FB_IMAP_LCD800X600)
#define IMAPFB_HFP		19		/* front porch */
#define IMAPFB_HSW		20		/* hsync width */
#define IMAPFB_HBP		46		/* back porch */

#define IMAPFB_VFP		8		/* front porch */
#define IMAPFB_VSW		4		/* vsync width */
#define IMAPFB_VBP		23		/* back porch */

#define IMAPFB_HRES		800		/* horizon pixel x resolition */
#define IMAPFB_VRES		600		/* line cnt y resolution */

#define IMAPFB_HRES_OSD	800		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD	600		/* line cnt y resolution */

#define IMAPFB_HRES_OSD_VIRTUAL	800		/* horizon pixel x resolition */
#define IMAPFB_VRES_OSD_VIRTUAL	600		/* line cnt y resolution */

#define IMAPFB_VFRAME_FREQ	60		/* frame rate freq */
#endif

#define IMAPFB_PIXEL_CLOCK	(IMAPFB_VFRAME_FREQ * (IMAPFB_HFP + IMAPFB_HSW + IMAPFB_HBP + IMAPFB_HRES) * (IMAPFB_VFP + IMAPFB_VSW + IMAPFB_VBP + IMAPFB_VRES))

imapfb_fimd_info_t imapfb_fimd = {
	.lcdcon1 = IMAP_LCDCON1_PNRMODE_TFTLCD | IMAP_LCDCON1_ENVID_DISABLE,
	.lcdcon2 = IMAP_LCDCON2_VBPD(IMAPFB_VBP - 1) | IMAP_LCDCON2_LINEVAL(IMAPFB_VRES - 1) | IMAP_LCDCON2_VFPD(IMAPFB_VFP - 1) | IMAP_LCDCON2_VSPW(IMAPFB_VSW - 1),
	.lcdcon3 = IMAP_LCDCON3_HBPD(IMAPFB_HBP - 1) | IMAP_LCDCON3_HOZVAL(IMAPFB_HRES - 1) | IMAP_LCDCON3_HFPD(IMAPFB_HFP - 1),
	.lcdcon4 = IMAP_LCDCON4_HSPW(IMAPFB_HSW - 1),
#ifdef CONFIG_IMAP_PRODUCTION
#if defined(CONFIG_FB_IMAP_LCD1024X600)
	.lcdcon5 = (6 << 24) | IMAP_LCDCON5_INVVCLK_FALLING_EDGE | IMAP_LCDCON5_INVVLINE_INVERTED | IMAP_LCDCON5_INVVFRAME_INVERTED | IMAP_LCDCON5_INVVD_NORMAL
#elif defined(CONFIG_FB_IMAP_LCD800X600)
	.lcdcon5 = (0x24 << 24) | IMAP_LCDCON5_INVVCLK_FALLING_EDGE | IMAP_LCDCON5_INVVLINE_INVERTED | IMAP_LCDCON5_INVVFRAME_INVERTED | IMAP_LCDCON5_INVVD_NORMAL
#endif
#else
	.lcdcon5 = (6 << 24) | IMAP_LCDCON5_INVVCLK_FALLING_EDGE | IMAP_LCDCON5_INVVLINE_INVERTED | IMAP_LCDCON5_INVVFRAME_INVERTED | IMAP_LCDCON5_INVVD_NORMAL
#endif
		| IMAP_LCDCON5_INVVDEN_NORMAL | IMAP_LCDCON5_INVPWREN_NORMAL | IMAP_LCDCON5_PWREN_ENABLE,

	.ovcdcr = IMAP_OVCDCR_IFTYPE_RGB,

#if defined (CONFIG_FB_IMAP_BPP8)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_8BPP_ARGB232 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_8BPP_ARGB232 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_8BPP_ARGB232 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_8BPP_ARGB232 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 8,
	.bytes_per_pixel = 1,
	
#elif defined (CONFIG_FB_IMAP_BPP16)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_16BPP_RGB565 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_16BPP_RGB565 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_16BPP_RGB565 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_16BPP_RGB565 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 16,
	.bytes_per_pixel = 2,
	
#elif defined (CONFIG_FB_IMAP_BPP18)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_18BPP_RGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_18BPP_RGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_18BPP_RGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_18BPP_RGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 18,
	.bytes_per_pixel = 4,

#elif defined (CONFIG_FB_IMAP_BPP19)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_19BPP_ARGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_19BPP_ARGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_19BPP_ARGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_19BPP_ARGB666 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 19,
	.bytes_per_pixel = 4,

#elif defined (CONFIG_FB_IMAP_BPP24)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_24BPP_RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_24BPP_RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_24BPP_RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_24BPP_RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 24,
	.bytes_per_pixel = 4,

#elif defined (CONFIG_FB_IMAP_BPP25)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_25BPP_ARGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_25BPP_ARGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_25BPP_ARGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_25BPP_ARGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 25,
	.bytes_per_pixel = 4,

#elif defined (CONFIG_FB_IMAP_BPP28)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_28BPP_A4RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_28BPP_A4RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_28BPP_A4RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_28BPP_A4RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 28,
	.bytes_per_pixel = 4,

#elif defined (CONFIG_FB_IMAP_BPP32)
	.ovcw0cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE
		| IMAP_OVCWxCR_BPPMODE_32BPP_A8RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw1cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_32BPP_A8RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw2cr = IMAP_OVCWxCR_BUFSEL_BUF0 | IMAP_OVCWxCR_BUFAUTOEN_DISABLE | IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE
		| IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE | IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 
		| IMAP_OVCWxCR_BLD_PIX_PLANE | IMAP_OVCWxCR_BPPMODE_32BPP_A8RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.ovcw3cr = IMAP_OVCWxCR_BITSWP_DISABLE | IMAP_OVCWxCR_BIT2SWP_DISABLE | IMAP_OVCWxCR_BIT4SWP_DISABLE | IMAP_OVCWxCR_BYTSWP_DISABLE
		| IMAP_OVCWxCR_HAWSWP_DISABLE | IMAP_OVCWxCR_ALPHA_SEL_1 | IMAP_OVCWxCR_BLD_PIX_PLANE
		| IMAP_OVCWxCR_BPPMODE_32BPP_A8RGB888 | IMAP_OVCWxCR_ENWIN_DISABLE,
	.bpp = 32,
	.bytes_per_pixel = 4,

#endif

	.ovcw0pcar = IMAP_OVCWxPCAR_LEFTTOPX(0) | IMAP_OVCWxPCAR_LEFTTOPY(0),
	.ovcw0pcbr = IMAP_OVCWxPCBR_RIGHTBOTX(IMAPFB_HRES_OSD - 1) | IMAP_OVCWxPCBR_RIGHTBOTY(IMAPFB_VRES_OSD - 1),
	.ovcw0cmr = IMAP_OVCWxCMR_MAPCOLEN_DISABLE,

#if (CONFIG_FB_IMAP_NUM > 1)
	.ovcw1pcar = IMAP_OVCWxPCAR_LEFTTOPX(0) | IMAP_OVCWxPCAR_LEFTTOPY(0),
	.ovcw1pcbr = IMAP_OVCWxPCBR_RIGHTBOTX(IMAPFB_HRES_OSD - 1) | IMAP_OVCWxPCBR_RIGHTBOTY(IMAPFB_VRES_OSD - 1),
	.ovcw1pccr = IMAP_OVCWxPCCR_ALPHA0_R(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA0_G(IMAPFB_MAX_ALPHA_LEVEL)
		| IMAP_OVCWxPCCR_ALPHA0_B(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA1_R(IMAPFB_MAX_ALPHA_LEVEL)
		| IMAP_OVCWxPCCR_ALPHA1_G(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA1_B(IMAPFB_MAX_ALPHA_LEVEL),
	.ovcw1cmr = IMAP_OVCWxCMR_MAPCOLEN_DISABLE,
#endif

#if (CONFIG_FB_IMAP_NUM > 2)	
	.ovcw2pcar = IMAP_OVCWxPCAR_LEFTTOPX(0) | IMAP_OVCWxPCAR_LEFTTOPY(0),
	.ovcw2pcbr = IMAP_OVCWxPCBR_RIGHTBOTX(IMAPFB_HRES_OSD - 1) | IMAP_OVCWxPCBR_RIGHTBOTY(IMAPFB_VRES_OSD - 1),
	.ovcw2pccr = IMAP_OVCWxPCCR_ALPHA0_R(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA0_G(IMAPFB_MAX_ALPHA_LEVEL)
		| IMAP_OVCWxPCCR_ALPHA0_B(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA1_R(IMAPFB_MAX_ALPHA_LEVEL)
		| IMAP_OVCWxPCCR_ALPHA1_G(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA1_B(IMAPFB_MAX_ALPHA_LEVEL),
	.ovcw2cmr = IMAP_OVCWxCMR_MAPCOLEN_DISABLE,
#endif

#if (CONFIG_FB_IMAP_NUM > 3)
	.ovcw3pcar = IMAP_OVCWxPCAR_LEFTTOPX(0) | IMAP_OVCWxPCAR_LEFTTOPY(0),
	.ovcw3pcbr = IMAP_OVCWxPCBR_RIGHTBOTX(IMAPFB_HRES_OSD - 1) | IMAP_OVCWxPCBR_RIGHTBOTY(IMAPFB_VRES_OSD - 1),
	.ovcw3pccr = IMAP_OVCWxPCCR_ALPHA0_R(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA0_G(IMAPFB_MAX_ALPHA_LEVEL)
		| IMAP_OVCWxPCCR_ALPHA0_B(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA1_R(IMAPFB_MAX_ALPHA_LEVEL)
		| IMAP_OVCWxPCCR_ALPHA1_G(IMAPFB_MAX_ALPHA_LEVEL) | IMAP_OVCWxPCCR_ALPHA1_B(IMAPFB_MAX_ALPHA_LEVEL),
	.ovcw3cmr = IMAP_OVCWxCMR_MAPCOLEN_DISABLE,
#endif	

	.sync = 0,
	.cmap_static = 1,

	.xres = IMAPFB_HRES,
	.yres = IMAPFB_VRES,

	.osd_xres = IMAPFB_HRES_OSD,
	.osd_yres = IMAPFB_VRES_OSD,

	.osd_xres_virtual = IMAPFB_HRES_OSD_VIRTUAL,
	.osd_yres_virtual = IMAPFB_VRES_OSD_VIRTUAL,

	.osd_xoffset = 0,
	.osd_yoffset = 0,

	.pixclock = IMAPFB_PIXEL_CLOCK,

	.hsync_len = IMAPFB_HSW,
	.vsync_len = IMAPFB_VSW,
	.left_margin = IMAPFB_HBP,
	.upper_margin = IMAPFB_VBP,
	.right_margin = IMAPFB_HFP,
	.lower_margin = IMAPFB_VFP,
	.set_lcd_power = imapfb_lcd_power_supply,
	.set_backlight_power= imapfb_backlight_power_supply,
	.set_brightness = imapfb_set_brightness,
};

void imapfb_lcd_power_supply(UINT32 on_off)
{
#if defined(CONFIG_FB_IMAP_LCD1024X600)

#elif defined(CONFIG_FB_IMAP_LCD800X600)

#if defined(CONFIG_IMAP_PRODUCTION_P0811A)
	if(on_off)
	{
		/* LCD Power ON */
		// LCD Power(GPO10) Enable control
		__raw_writel((((__raw_readl(rGPOCON)) & ~(3<<18)) | (1<<18)), rGPOCON);
		__raw_writel(((__raw_readl(rGPODAT)) | (1<<9)), rGPODAT);
		msleep(200);
	}
	else
	{
		/* LCD Power OFF*/
                 msleep(200);
                 __raw_writel((((__raw_readl(rGPOCON)) & ~(3<<18)) | (1<<18)), rGPOCON);
                 __raw_writel(((__raw_readl(rGPODAT)) & ~(1<<9)), rGPODAT);
	}

#elif defined(CONFIG_IMAP_PRODUCTION_P0811B)
	if(on_off)
	{
		/* LCD Power ON */
		// LCD Power(GPO10) Enable control
		__raw_writel((((__raw_readl(rGPOCON)) & ~(3<<18)) | (1<<18)), rGPOCON);
		__raw_writel(((__raw_readl(rGPODAT)) | (1<<9)), rGPODAT);
		msleep(200);
	}
	else
	{
		/* LCD Power OFF*/
                 msleep(200);
                 __raw_writel((((__raw_readl(rGPOCON)) & ~(3<<18)) | (1<<18)), rGPOCON);
                 __raw_writel(((__raw_readl(rGPODAT)) & ~(1<<9)), rGPODAT);
	}

#elif defined(CONFIG_IMAP_PRODUCTION_P0811C)

#endif

#endif
#if 0
#ifdef CONFIG_LCD_FOR_PRODUCT
	if(on_off)
	{
		/* LCD Power ON */
		// LCD Power(GPO10) Enable control
		__raw_writel((((__raw_readl(rGPOCON)) & ~(3<<20)) | (1<<20)), rGPOCON);
		__raw_writel(((__raw_readl(rGPOPUD)) | (1<<10)), rGPOPUD);
		__raw_writel(((__raw_readl(rGPODAT)) | (1<<10)), rGPODAT);
		msleep(60);
	}
	else
	{
		/* LCD Power OFF*/
                 msleep(60);
                 __raw_writel((((__raw_readl(rGPOCON)) & ~(3<<20)) | (1<<20)), rGPOCON);
                 __raw_writel(((__raw_readl(rGPOPUD)) | (1<<10)), rGPOPUD);
                 __raw_writel(((__raw_readl(rGPODAT)) & ~(1<<10)), rGPODAT);
         }
 #else
         if(on_off)
         {
                 /* LCD Power ON */
                 // LCD Power(GPF7) Enable control
                 __raw_writel((((__raw_readl(rGPFCON)) & ~(3<<14)) | (1<<14)), rGPFCON);
                 __raw_writel(((__raw_readl(rGPFPUD)) | (1<<7)), rGPFPUD);
                 __raw_writel(((__raw_readl(rGPFDAT)) | (1<<7)), rGPFDAT);
                 msleep(60);
         }
         else
         {
                 /* LCD Power OFF */
                 msleep(60);
                 __raw_writel((((__raw_readl(rGPFCON)) & ~(3<<14)) | (1<<14)), rGPFCON);
                 __raw_writel(((__raw_readl(rGPFPUD)) | (1<<7)), rGPFPUD);
                 __raw_writel(((__raw_readl(rGPFDAT)) & ~(1<<7)), rGPFDAT);
         }
#endif
#endif
}


void imapfb_backlight_power_supply(UINT32 on_off)
{
#if defined(CONFIG_FB_IMAP_LCD1024X600)

#elif defined(CONFIG_FB_IMAP_LCD800X600)

#if defined(CONFIG_IMAP_PRODUCTION_P0811A)
	if(on_off)
	{
                /* LCD Backlight ON */
		__raw_writel((((__raw_readl(rGPICON)) & ~(3<<12)) | (1<<12)), rGPICON);
		__raw_writel(((__raw_readl(rGPIDAT)) | (1<<6)), rGPIDAT);
		msleep(200);
	}
	else
	{
                /* LCD Backlight OFF */
                 msleep(200);
                 __raw_writel((((__raw_readl(rGPICON)) & ~(3<<12)) | (1<<12)), rGPICON);
                 __raw_writel(((__raw_readl(rGPIDAT)) & ~(1<<6)), rGPIDAT);
	}
#elif defined(CONFIG_IMAP_PRODUCTION_P0811B)
	if(on_off)
	{
                /* LCD Backlight ON */
		__raw_writel((((__raw_readl(rGPECON)) & ~(3<<20)) | (1<<20)), rGPECON);
		__raw_writel(((__raw_readl(rGPEDAT)) | (1<<10)), rGPEDAT);
		msleep(200);
	}
	else
	{
                /* LCD Backlight OFF */
                 msleep(200);
                 __raw_writel((((__raw_readl(rGPECON)) & ~(3<<20)) | (1<<20)), rGPECON);
                 __raw_writel(((__raw_readl(rGPEDAT)) & ~(1<<10)), rGPEDAT);
	}

#elif defined(CONFIG_IMAP_PRODUCTION_P0811C)

#endif

#endif

#if 0
#ifdef CONFIG_LCD_FOR_PRODUCT
        if(on_off)
        {
                /* LCD Backlight ON */
                // Backlight(GPO11)
                msleep(250);
                __raw_writel((((__raw_readl(rGPOCON)) & ~(3<<22)) | (1<<22)), rGPOCON);
                __raw_writel(((__raw_readl(rGPOPUD)) | (1<<11)), rGPOPUD);
                __raw_writel(((__raw_readl(rGPODAT)) | (1<<11)), rGPODAT);
        }
        else
        {
                /* LCD Backlight OFF */
                __raw_writel((((__raw_readl(rGPOCON)) & ~(3<<22)) | (1<<22)), rGPOCON);
                __raw_writel(((__raw_readl(rGPOPUD)) | (1<<11)), rGPOPUD);
                __raw_writel(((__raw_readl(rGPODAT)) & ~(1<<11)), rGPODAT);
                msleep(250);
        }
#else
        if(on_off)
        {
                /* LCD Backlight ON */
                // Backlight(GPF6)
                msleep(250);
                __raw_writel((((__raw_readl(rGPFCON)) & ~(3<<12)) | (1<<12)), rGPFCON);
                __raw_writel(((__raw_readl(rGPFPUD)) | (1<<6)), rGPFPUD);
                __raw_writel(((__raw_readl(rGPFDAT)) | (1<<6)), rGPFDAT);
        }
        else
        {
                /* LCD Backlight OFF */
                __raw_writel((((__raw_readl(rGPFCON)) & ~(3<<12)) | (1<<12)), rGPFCON);
                __raw_writel(((__raw_readl(rGPFPUD)) | (1<<6)), rGPFPUD);
                __raw_writel(((__raw_readl(rGPFDAT)) & ~(1<<6)), rGPFDAT);
                msleep(250);
        }
#endif
#endif
}

void imapfb_set_brightness(UINT32 val)
{
	printk(KERN_INFO "Set Lcd Backlight Brightness %d\n", val);
#if 0
	int channel = 0;	/* must use channel-1 */
	int usec = 10;	/* don't care value */
	unsigned long tcnt = 100;
	unsigned long tcmp = 0;
	unsigned int scale = 1;

	if (tcnt >= 100)
		scale = tcnt / 100;
	else
		scale = 100 / tcnt;
	
	if (val < 0)
		val = 0;

	if (val > S3CFB_MAX_BRIGHTNESS)
		val = S3CFB_MAX_BRIGHTNESS;

	s3cfb_fimd.brightness = val;
	if (tcnt >= 100)
		tcmp = val * scale;
	else
		tcmp = val / scale;

	msleep(50);
	s3c6400_timer_setup (channel, usec, tcnt, tcmp);
#endif
}

void imapfb_set_gpio(void)
{
	printk(KERN_INFO "LCD TYPE :: will be initialized\n");

	//Set RGB IF Data Line and Control Singal Line
	writel(0xaaaaaaaa, rGPMCON);
	writel(0x2aaaaaa, rGPNCON);
	writel(0, rGPMPUD);
	writel(0, rGPNPUD);
}

void imapfb_set_clk(void)
{
//	printk(KERN_INFO "IDS Clock Source Setting\n");

	//Set IF Clk Source and OSD Clk Source
#if defined(CONFIG_FB_IMAP_LCD1024X600)
	writel((7 << 10) | (2 << 8) | (7 << 2) | (2 << 0), rDIV_CFG4);
#elif defined(CONFIG_FB_IMAP_LCD800X600)
        unsigned int temp;

	temp = readl(rDPLL_CFG); 
	temp &=~(1<<31);
	writel(temp,rDPLL_CFG);

	temp = readl(rDPLL_CFG); 
	temp = 0x13;
	writel(temp,rDPLL_CFG);

	//enable dpll   
	temp = readl(rDPLL_CFG); 
	temp |=(1<<31);
	writel(temp,rDPLL_CFG);

	/*wait untill dpll is locked*/
	while(!(readl(rPLL_LOCKED) & 0x2));

	writel((4 << 10) | (2 << 8) | (4 << 2) | (2 << 0), rDIV_CFG4);
#endif
}

#if defined (CONFIG_FB_IMAP_KERNEL_LOGO)
void imapfb_kernel_logo(void *buf)
{
	int i, nX, nY;
	UINT8 *pDB;
	UINT8 *pFB;
	pFB = (UINT8 *)buf;
	
#if defined (CONFIG_FB_IMAP_BPP16)
	pDB = (UINT8 *)logo_300x120;

	memset(pFB, 0xff, IMAPFB_HRES * IMAPFB_VRES * 2);
	for (i = 0; i < IMAPFB_HRES * IMAPFB_VRES; i++)
	{
		nX = i % IMAPFB_HRES;
		nY = i / IMAPFB_HRES;
		if((nX >= ((IMAPFB_HRES - 300) / 2)) && (nX < ((IMAPFB_HRES + 300) / 2)) && (nY >= ((IMAPFB_VRES - 120) / 2)) && (nY < ((IMAPFB_VRES + 120) / 2)))
		{
			*pFB++ = *pDB++;
			*pFB++ = *pDB++;
		}
		else
		{
			pFB++;
			pFB++;
		}
	}
#elif defined (CONFIG_FB_IMAP_BPP32)
	pDB = (UINT8 *)gImage_logo;

	memset(pFB, 0xff, IMAPFB_HRES * IMAPFB_VRES * 4);
	for (i = 0; i < IMAPFB_HRES * IMAPFB_VRES; i++)
	{
		nX = i % IMAPFB_HRES;
		nY = i / IMAPFB_HRES;
		if((nX >= ((IMAPFB_HRES - 320) / 2)) && (nX < ((IMAPFB_HRES + 320) / 2)) && (nY >= ((IMAPFB_VRES - 240) / 2)) && (nY < ((IMAPFB_VRES + 240) / 2)))
		{
			*pFB++ = *pDB++;
			*pFB++ = *pDB++;
			*pFB++ = *pDB++;
			*pFB++ = *pDB++;
		}
		else
		{
			pFB++;
			pFB++;
			pFB++;
			pFB++;
		}
	}
#endif
}
#endif
