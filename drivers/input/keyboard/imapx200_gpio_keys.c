/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <asm/io.h>
#include <asm-generic/bitops/non-atomic.h>
#include <asm/gpio.h>
#include <mach/imapx_gpio.h>
#include <mach/irqs.h>
#include <mach/imapx_intr.h>
#include <mach/imapx_sysmgr.h>

#ifdef CONFIG_FAKE_PM
#include <plat/fake_pm.h>
extern struct completion power_key;
#endif

//#define  IMAPX200_PRODUCT  1  

extern struct completion Menu_Button;
extern unsigned int HDMI_MODE;
static volatile int report_as_sleep = 1;
static struct gpio_keys_drvdata *ddata;
static int key_back = 0;
static int key_power = 0;
static unsigned long time_back_sec_begin, time_back_sec_end, \
		     time_back_usec_begin, time_back_usec_end,\
		     time_menu_sec_begin, time_menu_sec_end,\
		     time_menu_usec_begin, time_menu_usec_end,\
		     time_down_sec_begin, time_down_sec_end,\
		     time_down_usec_begin, time_down_usec_end,\
		     time_up_sec_begin, time_up_sec_end,\
		     time_up_usec_begin, time_up_usec_end,\
		     time_home_sec_begin, time_home_sec_end,\
		     time_home_usec_begin, time_home_usec_end;
struct gpio_button_data {
	struct gpio_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work;
};

struct gpio_keys_drvdata {
	struct input_dev *input;
	struct gpio_button_data data[0];
};

int imapx200_gpio_get_value(unsigned gpio)
{
//	unsigned long gpio_value;
	return 1;
//	return chip->get ? chip->get(chip, gpio - chip->base) : 0;
}
/*******************************************************/
static void imap_irq_enable (unsigned int irqno)
{
	unsigned long mask;

		mask = __raw_readl(rINTMSK);
		mask &= ~(1UL << irqno);
		__raw_writel(mask, rINTMSK);

}

static void imap_irq_disable (unsigned int irqno)
{
        unsigned long mask, bitval;
                mask = __raw_readl(rINTMSK);
                mask |= 1UL << irqno;
                __raw_writel(mask, rINTMSK);
	
		bitval = (1UL << irqno);
		__raw_writel(bitval, rINTPND);
		__raw_writel(0x0, rSRCPND);
}


/*******************************************************/


int imapx200_gpio_to_irq(unsigned gpio)
{
	return gpio;
	//return chip->to_irq ? chip->to_irq(chip, gpio - chip->base) : -ENXIO;
}

int imapx200_gpio_key_emulate(unsigned char c)
{
	struct gpio_keys_button *button = ddata->data[0].button;
	struct input_dev *input = ddata->data[0].input;
	unsigned int type = button->type ?: EV_KEY;
	input_event(input, type, c, 1);
	input_event(input, type, c, 0);
	return 0;
}

EXPORT_SYMBOL(imapx200_gpio_key_emulate);
static void gpio_keys_report_event(struct work_struct *work)
{
	struct gpio_button_data *bdata =
		container_of(work, struct gpio_button_data, work);
	struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?: EV_KEY;
	unsigned int code = button->code;
         unsigned long bitval1;   
         unsigned int irqno;
	int tmp,tmp1;
	struct timeval end_time;
//	int state = (imapx200_gpio_get_value(button->gpio) ? 1 : 0) ^ button->active_low;

	/* Modified for android, by warits */
	/* This is a trick where state 0 is send first to clear
	   state set last time */
	if (code == KEY_POWER) {
//		key_power = 0;
#ifdef CONFIG_FAKE_PM
		if(if_in_suspend == 1)
		{
			complete(&power_key);
		}
#endif
//		input_event(input, type, 116, 0);
		input_event(input, type, 116, 1);
		if(report_as_sleep)
			input_event(input, type, 116, 0);
	} else if(code == KEY_MENU) {
		if (key_back == 1) {
			/******************************************************/
			do_gettimeofday(&end_time);
			time_up_sec_begin = end_time.tv_sec;
			time_up_usec_begin = end_time.tv_usec;
			if (time_up_sec_begin*1000000+time_up_usec_begin - \
					time_up_sec_end*1000000-time_up_usec_end > 150000) {
			//	printk(KERN_DEBUG"*********send key_volumeup!\n");
				input_event(input, type, KEY_VOLUMEUP, 1);
				input_event(input, type, KEY_VOLUMEUP, 0);
			}
			else {
			}
			do_gettimeofday(&end_time);
			time_up_sec_end = end_time.tv_sec;
			time_up_usec_end = end_time.tv_usec;
			/***************************************************/
		}else if (key_back ==2){
			/******************************************************/
			do_gettimeofday(&end_time);
			time_down_sec_begin = end_time.tv_sec;
			time_down_usec_begin = end_time.tv_usec;
			if (time_down_sec_begin*1000000+time_down_usec_begin - \
					time_down_sec_end*1000000-time_down_usec_end > 150000) {

			//	printk(KERN_DEBUG"*********send key_volumedown!\n");
				input_event(input, type, KEY_VOLUMEDOWN, 1);
				input_event(input, type, KEY_VOLUMEDOWN, 0);
			}
			else {
			}
			do_gettimeofday(&end_time);
			time_down_sec_end = end_time.tv_sec;
			time_down_usec_end = end_time.tv_usec;
			/***************************************************/
		}else if (key_back ==3){
			/******************************************************/
			do_gettimeofday(&end_time);
			time_menu_sec_begin = end_time.tv_sec;
			time_menu_usec_begin = end_time.tv_usec;
			if (time_menu_sec_begin*1000000+time_menu_usec_begin - \
					time_menu_sec_end*1000000-time_menu_usec_end > 150000) {

			//	printk(KERN_DEBUG"*********send key_menu!\n");
				input_event(input, type, KEY_MENU, 1);
				input_event(input, type, KEY_MENU, 0);
#ifdef CONFIG_HDMI_OUTPUT_SUPPORT

#ifdef CONFIG_FAKE_PM
				if(if_in_suspend == 0)
				{
#endif
					complete(&Menu_Button);
#ifdef CONFIG_FAKE_PM
				}
#endif

#endif
			}
			else {
			}
			do_gettimeofday(&end_time);
			time_menu_sec_end = end_time.tv_sec;
			time_menu_usec_end = end_time.tv_usec;
			/***************************************************/
		}else if (key_back ==4){
			/******************************************************/
			do_gettimeofday(&end_time);
			time_home_sec_begin = end_time.tv_sec;
			time_home_usec_begin = end_time.tv_usec;
			if (time_home_sec_begin*1000000+time_home_usec_begin - \
					time_home_sec_end*1000000-time_home_usec_end > 150000) {

			//	printk(KERN_DEBUG"*********send key_home!\n");
				if(!HDMI_MODE)
				{
					input_event(input, type, KEY_HOME, 1);
					input_event(input, type, KEY_HOME, 0);
				}
			}
			else {
			}
			do_gettimeofday(&end_time);
			time_home_sec_end = end_time.tv_sec;
			time_home_usec_end = end_time.tv_usec;
			/***************************************************/
		}else if (key_back ==5){
#if CONFIG_IMAP_PRODUCTION_P1011B
			/********************************************/
			msleep(500);
			tmp = __raw_readl(rGPCDAT);
			tmp1 = __raw_readl(rGPPDAT); //gpp2
			if (tmp & (1<<7)){         
				printk(KERN_DEBUG "louderspeaker enable!\n");
				if(!HDMI_MODE)
				{
					tmp1 |= 0x1<<2;
					__raw_writel(tmp1, rGPPDAT);  //louderspeaker enable
				}
			}else {
				printk(KERN_DEBUG "louderspeaker disable!\n");
				tmp1 &= ~(0x1<<2);
				__raw_writel(tmp1, rGPPDAT);  //louderspeaker disable
			}
			/***************************************/
#elif CONFIG_IMAP_PRODUCTION_P0811B
			/********************************************/
			msleep(500);
			tmp = __raw_readl(rGPCDAT);
			tmp1 = __raw_readl(rGPPDAT); //gpp2
			if (tmp & (1<<7)){         
				printk(KERN_DEBUG "louderspeaker enable!\n");
				if(!HDMI_MODE)
				{
					tmp1 |= 0x1<<2;
					__raw_writel(tmp1, rGPPDAT);  //louderspeaker enable
				}
			}else {
				printk(KERN_DEBUG "louderspeaker disable!\n");
				tmp1 &= ~(0x1<<2);
				__raw_writel(tmp1, rGPPDAT);  //louderspeaker disable
			}
			/***************************************/
#elif CONFIG_IMAP_PRODUCTION_P1011C
#else
#endif
		}

	}else if (code == KEY_BACK){

//		tmp = __raw_readl(rGPGDAT)&0x20;
		do_gettimeofday(&end_time);
		time_back_sec_begin = end_time.tv_sec;
		time_back_usec_begin = end_time.tv_usec;
		if (time_back_sec_begin*1000000+time_back_usec_begin - \
				time_back_sec_end*1000000-time_back_usec_end > 150000) {
		//	printk(KERN_DEBUG"*********send key_back!\n");
			input_event(input, type, KEY_BACK, 1);
			input_event(input, type, KEY_BACK, 0);
		}
		else {
		}
		do_gettimeofday(&end_time);
		time_back_sec_end = end_time.tv_sec;
		time_back_usec_end = end_time.tv_usec;
	}

	//	clear_bit(2, input->key);
}

static void gpio_keys_timer(unsigned long _data)
{
	struct gpio_button_data *data = (struct gpio_button_data *)_data;

	schedule_work(&data->work);
}

static irqreturn_t gpio_back_keys_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;
	struct gpio_keys_button *button = bdata->button;
	schedule_work(&bdata->work);
	return IRQ_HANDLED;
}
#ifdef CONFIG_SWITCH_WIFI
extern struct work_struct switch_wifi_work;
#endif
static irqreturn_t gpio_menu_keys_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;
	struct gpio_keys_button *button = bdata->button;
	unsigned long tmp, tmp1, pndval,pndval4, pndval3;
	unsigned long mask; 
	unsigned long bitval1;  
	unsigned int irqno;
#ifdef CONFIG_SWITCH_WIFI
	if(readl(rEINTG4PEND) & (0x1 << 8))
	{
		writel(0x1 << 8, rEINTG4PEND);
		schedule_work(&switch_wifi_work);
		return IRQ_HANDLED;
	}
#endif
#ifdef CONFIG_IMAP_PRODUCTION_P1011A
	tmp = __raw_readl(rEINTG6MASK);
	tmp |= (0xf<<11);
	__raw_writel(tmp, rEINTG6MASK);
	tmp=__raw_readl(rEINTG4MASK);
	tmp|=(0x1<<31);
	__raw_writel( tmp,rEINTG4MASK); 


	pndval4=__raw_readl(rEINTG4PEND);


	pndval = __raw_readl(rEINTG6PEND);
	//	printk(KERN_DEBUG"pndval = %08x,pndval4=%08x\n", pndval,pndval4);
#elif CONFIG_IMAP_PRODUCTION_P1011B       
	pndval4 =__raw_readl(rEINTG4PEND);
	pndval = __raw_readl(rEINTG1PEND);
	pndval3 = __raw_readl(rEINTG3PEND);
#elif CONFIG_IMAP_PRODUCTION_P0811B       
	pndval4 =__raw_readl(rEINTG4PEND);
	pndval = __raw_readl(rEINTG1PEND);
	pndval3 = __raw_readl(rEINTG3PEND);
#elif CONFIG_IMAP_PRODUCTION_P1011C
#else
#endif
	if(pndval4 & (1<<31))
	{

#ifdef CONFIG_IMAP_PRODUCTION_P1011B
		__raw_writel((1<<31), rEINTG4PEND);
		if (!(readl(rGPODAT) & (0x1 << 11)))
		{
			return IRQ_HANDLED;
		}
		else
		{
			tmp=__raw_readl(rEINTG4MASK);
			tmp|=(0x1<<31);
			__raw_writel( tmp,rEINTG4MASK); 
			key_back = 3;
			schedule_work(&bdata->work);
			tmp = __raw_readl(rEINTG4MASK);
			tmp &= ~(0x1<<31);
			__raw_writel(tmp, rEINTG4MASK);
		}
#endif
#ifdef CONFIG_IMAP_PRODUCTION_P0811B
		__raw_writel((1<<31), rEINTG4PEND);
		if (!(readl(rGPODAT) & (0x1 << 11)))
		{
			return IRQ_HANDLED;
		}
		else
		{
			tmp=__raw_readl(rEINTG4MASK);
			tmp|=(0x1<<31);
			__raw_writel( tmp,rEINTG4MASK); 
			key_back = 1;
			schedule_work(&bdata->work);
			tmp = __raw_readl(rEINTG4MASK);
			tmp &= ~(0x1<<31);
			__raw_writel(tmp, rEINTG4MASK);
		}
#endif
	}
#ifdef CONFIG_IMAP_PRODUCTION_P1011B
	if(pndval4 & (1<<14))
	{


		__raw_writel((1<<14), rEINTG4PEND);
		if (!(readl(rGPEDAT) & (0x1 << 14)))
		{
			return IRQ_HANDLED;
		}
		else
		{
			tmp=__raw_readl(rEINTG4MASK);
			tmp|=(0x1<<14);
			__raw_writel( tmp,rEINTG4MASK); 
			key_back = 1;
			schedule_work(&bdata->work);
			tmp = __raw_readl(rEINTG4MASK);
			tmp &= ~(0x1<<14);
			__raw_writel(tmp, rEINTG4MASK);
		}
	}
#endif
#ifdef CONFIG_IMAP_PRODUCTION_P0811B
	if(pndval4 & (1<<14))
	{


		__raw_writel((1<<14), rEINTG4PEND);
		if (!(readl(rGPEDAT) & (0x1 << 14)))
		{
			return IRQ_HANDLED;
		}
		else
		{
			tmp=__raw_readl(rEINTG4MASK);
			tmp|=(0x1<<14);
			__raw_writel( tmp,rEINTG4MASK); 
			key_back = 3;
			schedule_work(&bdata->work);
			tmp = __raw_readl(rEINTG4MASK);
			tmp &= ~(0x1<<14);
			__raw_writel(tmp, rEINTG4MASK);
		}
	}
#endif
#ifdef CONFIG_IMAP_PRODUCTION_P1011A
	else if (pndval & (1<<11))
	{
		__raw_writel((1<<11), rEINTG6PEND);
		key_back = 1;
		schedule_work(&bdata->work);
	}
	else if (pndval & (1<<12))
	{
		__raw_writel((1<<12), rEINTG6PEND);
		key_back = 2;
		schedule_work(&bdata->work);
	}
	else if (pndval & (1<<13))
	{
		__raw_writel((1<<13), rEINTG6PEND);
		key_back = 3;
		schedule_work(&bdata->work);
	}
	else if (pndval & (1<<14))
	{
		__raw_writel((1<<14), rEINTG6PEND);
		key_back = 4;
		schedule_work(&bdata->work);
	}
	else{
		//__raw_writel(pndval, rEINTG6PEND);
		printk(KERN_INFO "Undefined External Interrupt of GPIO group!\n");
	}
	tmp = __raw_readl(rEINTG6MASK);
	tmp &= ~(0xf<<11);
	__raw_writel(tmp, rEINTG6MASK);
#elif CONFIG_IMAP_PRODUCTION_P1011B
	else if (pndval3 & (1<<7))
	{
		__raw_writel((1<<7), rEINTG3PEND);
		tmp = __raw_readl(rEINTG3MASK);
		tmp |= (1<<7);
		__raw_writel(tmp, rEINTG3MASK);
		key_back = 5;
		schedule_work(&bdata->work);
		/********************************************/
		/*
		   tmp = __raw_readl(rGPCDAT);
		   tmp1 = __raw_readl(rGPPDAT); //gpp2
		   if (tmp & (1<<7)){         
		   printk(KERN_DEBUG "louderspeaker enable!\n");
		   tmp1 |= 0x1<<2;
		   __raw_writel(tmp1, rGPPDAT);  //louderspeaker enable
		   }
		   else {
		   printk(KERN_DEBUG "louderspeaker disable!\n");
		   tmp1 &= ~(0x1<<2);
		   __raw_writel(tmp1, rGPPDAT);  //louderspeaker disable
		   }
		   */
		/***************************************/
		tmp = __raw_readl(rEINTG3MASK);
		tmp &= ~(1<<7);
		__raw_writel(tmp, rEINTG3MASK);

	}
	else if (pndval & (1<<5))
	{
		__raw_writel((1<<5), rEINTG1PEND);
		tmp = __raw_readl(rEINTG1MASK);
		tmp |= (1<<5);
		__raw_writel(tmp, rEINTG1MASK);

		key_back = 2;//home
		schedule_work(&bdata->work);
		tmp = __raw_readl(rEINTG1MASK);
		tmp &= ~(1<<5);
		__raw_writel(tmp, rEINTG1MASK);
	}
	else if (pndval & (1<<7))
	{
		__raw_writel((1<<7), rEINTG1PEND);
		tmp = __raw_readl(rEINTG1MASK);
		tmp |= (1<<7);
		__raw_writel(tmp, rEINTG1MASK);
		key_back = 4;         //vol-
		schedule_work(&bdata->work);
		tmp = __raw_readl(rEINTG1MASK);
		tmp &= ~(1<<5);
		tmp &= ~(1<<7);
		__raw_writel(tmp, rEINTG1MASK);
	}
	else{
		//__raw_writel(pndval, rEINTG6PEND);
		printk(KERN_INFO "Undefined External Interrupt of GPIO group!\n");
	}
#elif CONFIG_IMAP_PRODUCTION_P0811B
	else if (pndval3 & (1<<7))
	{
		__raw_writel((1<<7), rEINTG3PEND);
		tmp = __raw_readl(rEINTG3MASK);
		tmp |= (1<<7);
		__raw_writel(tmp, rEINTG3MASK);
		key_back = 5;
		schedule_work(&bdata->work);
		/********************************************/
		/*
		   tmp = __raw_readl(rGPCDAT);
		   tmp1 = __raw_readl(rGPPDAT); //gpp2
		   if (tmp & (1<<7)){         
		   printk(KERN_DEBUG "louderspeaker enable!\n");
		   tmp1 |= 0x1<<2;
		   __raw_writel(tmp1, rGPPDAT);  //louderspeaker enable
		   }
		   else {
		   printk(KERN_DEBUG "louderspeaker disable!\n");
		   tmp1 &= ~(0x1<<2);
		   __raw_writel(tmp1, rGPPDAT);  //louderspeaker disable
		   }
		   */
		/***************************************/
		tmp = __raw_readl(rEINTG3MASK);
		tmp &= ~(1<<7);
		__raw_writel(tmp, rEINTG3MASK);

	}
	else if (pndval & (1<<5))
	{
		__raw_writel((1<<5), rEINTG1PEND);
		tmp = __raw_readl(rEINTG1MASK);
		tmp |= (1<<5);
		__raw_writel(tmp, rEINTG1MASK);

		key_back = 2;//home
		schedule_work(&bdata->work);
		tmp = __raw_readl(rEINTG1MASK);
		tmp &= ~(1<<5);
		__raw_writel(tmp, rEINTG1MASK);
	}
	else if (pndval & (1<<7))
	{
		__raw_writel((1<<7), rEINTG1PEND);
		tmp = __raw_readl(rEINTG1MASK);
		tmp |= (1<<7);
		__raw_writel(tmp, rEINTG1MASK);
		key_back = 4;         //vol-
		schedule_work(&bdata->work);
		tmp = __raw_readl(rEINTG1MASK);
		tmp &= ~(1<<5);
		tmp &= ~(1<<7);
		__raw_writel(tmp, rEINTG1MASK);
	}
	else{
		//__raw_writel(pndval, rEINTG6PEND);
		printk(KERN_INFO "Undefined External Interrupt of GPIO group!\n");
	}
#elif CONFIG_IMAP_PRODUCTION_P1011C
#else
#endif
	return IRQ_HANDLED;
}


static irqreturn_t gpio_keys_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;
	struct gpio_keys_button *button = bdata->button;
	volatile unsigned int tmp;
	tmp = __raw_readl(rPOW_STB);
	printk(KERN_DEBUG "tmp is %d\n", tmp);

	if (tmp & 0x1) {
		while(tmp != 0){
		__raw_writel(0x1, rPOW_STB);
		udelay(300);
		tmp = __raw_readl(rPOW_STB);
		}
#ifdef CONFIG_FAKE_PM
		report_as_sleep = 1;
		goto isr_sc_work;
#else
		report_as_sleep = 1;
		goto isr_sc_work;
#endif
	}
	else if (tmp & 0x2){


		while(tmp != 0){
		__raw_writel(0x2, rPOW_STB);
		udelay(300);
		tmp = __raw_readl(rPOW_STB);
		}

		printk(KERN_INFO "State 0x2\n");
//		key_power = 1;
		report_as_sleep = 0;
		goto isr_sc_work;
	}
	else if (tmp & 0x4)
	{
		__raw_writel(0x4, rPOW_STB);
		__raw_writel(0xff, rWP_MASK);
		__raw_writel(0x4, rGPOW_CFG);
		goto irqreturn;
//		goto isr_sc_work;
	}
	else{
		printk(KERN_ERR "this is the wron value, %d!\n", tmp);
	}	

//	printk(KERN_DEBUG "State 0x1\n");
//	report_as_sleep = 1;
isr_sc_work:
#if 1
	if (button->debounce_interval)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(button->debounce_interval));
	else
		schedule_work(&bdata->work);
#endif

irqreturn:
	return IRQ_HANDLED;
}

#ifdef CONFIG_IMAP_PRODUCTION
#ifdef CONFIG_IMAP_PRODUCTION_P1011A
static irqreturn_t gpio_keys_audio(int irq, void *dev_id)
{

	/***********************************************/
	unsigned long rGPGDAT_value, rGPIDAT_value;

	rGPGDAT_value = __raw_readl(rGPGDAT);
	rGPIDAT_value = __raw_readl(rGPIDAT);
	if (rGPGDAT_value & (1<<0)){         
		printk(KERN_DEBUG "louderspeaker enable!\n");
		rGPIDAT_value |= 0x1<<4;
		__raw_writel(rGPIDAT_value, rGPIDAT);  //louderspeaker enable
	}
	
	else {
		printk(KERN_DEBUG "louderspeaker disable!\n");
		rGPIDAT_value &= ~(0x1<<4);
		__raw_writel(rGPIDAT_value, rGPIDAT);  //louderspeaker disable
	}
	/**********************************************/
	return IRQ_HANDLED;
}
#endif
#endif


static int __devinit gpio_keys_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	int i, error;
	int wakeup = 0;

	unsigned long tmp;

/******************************************************************/
#ifdef CONFIG_IMAP_PRODUCTION
	unsigned long rEINTGCON_value, rEINTGFLTCON1_value, rGPFCON_value, rGPFDAT_value, rGPGDAT_value;
#ifdef CONFIG_FB_IMAP_LCD1024X600
#ifdef CONFIG_IMAP_PRODUCTION_P1011A
	/*wifi switch*/
	tmp = __raw_readl(rGPRCON);
	tmp &= ~(0x3 << 28);
	__raw_writel(tmp , rGPRCON);
#elif CONFIG_IMAP_PRODUCTION_P1011B
	//tmp = __raw_readl(rGPJCON);
	//tmp &= ~(0x3 << 8);
	//tmp |= (0x1 << 8);
	//__raw_writel(tmp , rGPJCON);

	//tmp = __raw_readl(rGPJDAT);
	//tmp |= (0x1 << 4);
	//__raw_writel(tmp , rGPJDAT);
#endif
#elif CONFIG_FB_IMAP_LCD800X600
	tmp = __raw_readl(rGPRCON);
	tmp &= ~(0x3 << 28);
	tmp |= (0x1 << 28);
	__raw_writel(tmp , rGPRCON);

	tmp = __raw_readl(rGPRDAT);
	tmp |= (0x1 << 14);
	__raw_writel(tmp , rGPRDAT);
#endif
#ifdef CONFIG_IMAP_PRODUCTION_P1011A
	/*eint0,gpg0 as the audio switch and eint0 as the key_back*/

	/*config eint0  and eint1 as falling edge interrupt*/
	tmp = __raw_readl(rEINTCON);
	tmp &= ~(0x7<<0);
	tmp |= (0x7<<0);  //eint0
	tmp &= ~(0x7<<4);
	tmp |= (0x2<<4);  //eint1
	__raw_writel(tmp, rEINTCON);
	tmp = __raw_readl(rEINTFLTCON0);
	tmp |= 0xff;   //eint0
	tmp |= (0xff<<8);   //eint1
	__raw_writel(tmp, rEINTFLTCON0);

	/****config GPI7-GPI10 as eint group6*****/
//gpi7 as vol+, gpi8 as home, gpi9 as menu, gpi10 as vol-	
	tmp = __raw_readl(rGPICON);
	tmp &= ~(0xff<<14);
	tmp &= ~(0x3<<8);
	tmp |= (0x1<<8);
	__raw_writel(tmp, rGPICON);
	tmp = __raw_readl(rGPIPUD);
	tmp |= (0xf<<7);
	__raw_writel(tmp, rGPIPUD);

	rEINTGCON_value = __raw_readl(rEINTGCON);
	rEINTGCON_value &= ~(0x7<<20);
	rEINTGCON_value |= (0x3<<20);
	__raw_writel(rEINTGCON_value, rEINTGCON);
	rEINTGFLTCON1_value = __raw_readl(rEINTGFLTCON1);
	rEINTGFLTCON1_value |= (0xff<<8);
	__raw_writel(rEINTGFLTCON1_value, rEINTGFLTCON1);

	tmp = __raw_readl(rEINTG6MASK);
	tmp &= ~(0xf<<11);
	__raw_writel(tmp, rEINTG6MASK);

	/****************************************/
	rGPGDAT_value = __raw_readl(rGPGDAT);
	if (rGPGDAT_value & (1<<0)){         
		printk(KERN_INFO "---Headphone is not alive!\n");
		rGPFDAT_value = __raw_readl(rGPIDAT); //XKBC0L[4]
		rGPFDAT_value |= 0x1<<4;
		__raw_writel(rGPFDAT_value, rGPIDAT);  //louderspeaker enable
	}
	error = request_irq(IRQ_EINT0, gpio_keys_audio,
				    IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_DISABLED,
				     "gpio_keys_audio",NULL);
	if (error) {
		pr_err("gpio-keys: Unable to claim irq %d; error %d\n",
			IRQ_EINT0, error);
		return error;
		}
#elif CONFIG_IMAP_PRODUCTION_P1011B
	/*gpc7 as the audio switch*/

	/*config gpc7/eint group3[7] falling edge interrupt*/
	tmp = __raw_readl(rGPCCON);
	tmp &= ~(0x3<<0);
	__raw_writel(tmp, rGPCCON);
	tmp = __raw_readl(rEINTGCON);
	tmp &= ~(7<<8);
	tmp |= (7<<8);
	__raw_writel(tmp, rEINTGCON);
	tmp = __raw_readl(rEINTGFLTCON0);
	tmp |= (0xff<<16);   //eint group 3
	__raw_writel(tmp, rEINTGFLTCON0);
	
	tmp = __raw_readl(rEINTG3MASK);
	tmp &= ~(1<<7);
	__raw_writel(tmp, rEINTG3MASK);
//gpg5 as back key, gpo11/eint group4[31] as menu key, gpa5/eint group1[5] as home key, gpp0 as vol+, gpa7/eint group1[7] as vol-	
	tmp = __raw_readl(rGPECON);//gpe14/eint group4[14] as vol+
	tmp &= ~(0x3 << 28);
	__raw_writel(tmp, rGPECON);

	tmp = __raw_readl(rGPACON);
	tmp &= ~(0x3<<10);  //gpa5
	tmp &= ~(0x3<<14);  //gpa7
	__raw_writel(tmp, rGPACON);
	tmp = __raw_readl(rGPOCON);
	tmp &= ~(0x3<<22);  //gpo11
	__raw_writel(tmp, rGPOCON);
	
	tmp = __raw_readl(rEINTGCON);
        tmp &= ~(0x7<<0); 	
	tmp |= (4<<0);  //eint1 group
	tmp &= ~(0x7<<12);
	tmp |= (5<<12);  //eint4 group
	__raw_writel(tmp, rEINTGCON);

	tmp = __raw_readl(rEINTGFLTCON0);
	tmp |= (0xff<<0); //for eint1 group filter
	tmp |= (0xff<<24); //for eint4 group filter
	__raw_writel(tmp, rEINTGFLTCON0);
	tmp = __raw_readl(rEINTG1MASK);
	tmp &= ~(1<<5);
	tmp &= ~(1<<7);
	__raw_writel(tmp, rEINTG1MASK);
	tmp = __raw_readl(rEINTG4MASK);
	tmp &= ~(1<<31);
	tmp &= ~(1 << 14);
	__raw_writel(tmp, rEINTG4MASK);


	tmp = __raw_readl(rEINTCON);
	tmp &= ~(0x7<<20);
	tmp |= (0x4<<20); //gpg5
	__raw_writel(tmp, rEINTCON);

	tmp = __raw_readl(rEINTFLTCON1);
	tmp |= (0xff<<8); //for eint5 filter
	__raw_writel(tmp, rEINTFLTCON1);
	/****************************************/
	rGPGDAT_value = __raw_readl(rGPCDAT);
	tmp = __raw_readl(rGPPCON);
	tmp &= ~(3<<4);
	tmp |= (1<<4);
	__raw_writel(tmp, rGPPCON);

	if (rGPGDAT_value & (1<<7)){         
		printk(KERN_INFO "---Headphone is not alive!\n");
		rGPFDAT_value = __raw_readl(rGPPDAT); //gpp2
		rGPFDAT_value |= 0x1<<2;
		__raw_writel(rGPFDAT_value, rGPPDAT);  //louderspeaker enable
	}
#elif CONFIG_IMAP_PRODUCTION_P0811B
	/*gpc7 as the audio switch*/

	/*config gpc7/eint group3[7] falling edge interrupt*/
	tmp = __raw_readl(rGPCCON);
	tmp &= ~(0x3<<0);
	__raw_writel(tmp, rGPCCON);
	tmp = __raw_readl(rEINTGCON);
	tmp &= ~(7<<8);
	tmp |= (7<<8);
	__raw_writel(tmp, rEINTGCON);
	tmp = __raw_readl(rEINTGFLTCON0);
	tmp |= (0xff<<16);   //eint group 3
	__raw_writel(tmp, rEINTGFLTCON0);
	
	tmp = __raw_readl(rEINTG3MASK);
	tmp &= ~(1<<7);
	__raw_writel(tmp, rEINTG3MASK);
//gpg5 as back key, gpo11/eint group4[31] as menu key, gpa5/eint group1[5] as home key, gpe14/eint4[14] as vol+, gpa7/eint group1[7] as vol-	
	tmp = __raw_readl(rGPACON);
	tmp &= ~(0x3<<10);  //gpa5
	tmp &= ~(0x3<<14);  //gpa7
	__raw_writel(tmp, rGPACON);
	tmp = __raw_readl(rGPOCON);
	tmp &= ~(0x3<<22);  //gpo11
	__raw_writel(tmp, rGPOCON);
	
	tmp = __raw_readl(rEINTGCON);
        tmp &= ~(0x7<<0); 	
	tmp |= (4<<0);  //eint1 group
	tmp &= ~(0x7<<12);
	tmp |= (5<<12);  //eint4 group
	__raw_writel(tmp, rEINTGCON);

	tmp = __raw_readl(rEINTGFLTCON0);
	tmp |= (0xff<<0); //for eint1 group filter
	tmp |= (0xff<<24); //for eint4 group filter
	__raw_writel(tmp, rEINTGFLTCON0);
	tmp = __raw_readl(rEINTG1MASK);
	tmp &= ~(1<<5);
	tmp &= ~(1<<7);
	__raw_writel(tmp, rEINTG1MASK);
	tmp = __raw_readl(rEINTG4MASK);
	tmp &= ~(1<<31);
	tmp &= ~(1<<14);
	__raw_writel(tmp, rEINTG4MASK);


	tmp = __raw_readl(rEINTCON);
	tmp &= ~(0x7<<20);
	tmp |= (0x4<<20); //gpg5
	__raw_writel(tmp, rEINTCON);

	tmp = __raw_readl(rEINTFLTCON1);
	tmp |= (0xff<<8); //for eint5 filter
	__raw_writel(tmp, rEINTFLTCON1);
	/****************************************/
	rGPGDAT_value = __raw_readl(rGPCDAT);
	tmp = __raw_readl(rGPPCON);
	tmp &= ~(3<<4);
	tmp |= (1<<4);
	__raw_writel(tmp, rGPPCON);

	if (rGPGDAT_value & (1<<7)){         
		printk(KERN_INFO "---Headphone is not alive!\n");
		rGPFDAT_value = __raw_readl(rGPPDAT); //gpp2
		rGPFDAT_value |= 0x1<<2;
		__raw_writel(rGPFDAT_value, rGPPDAT);  //louderspeaker enable
	}
#elif CONFIG_PRODUCTION_P1011C
#else
#endif

#endif	

/*****************************************************************/


	ddata = kzalloc(sizeof(struct gpio_keys_drvdata) +
			pdata->nbuttons * sizeof(struct gpio_button_data),
			GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		error = -ENOMEM;
		goto fail1;
	}

	platform_set_drvdata(pdev, ddata);

	input->name = pdev->name;
	input->phys = "gpio-keys/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
//	if (pdata->rep)
//		__set_bit(EV_REP, input->evbit);

	ddata->input = input;

	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_keys_button *button = &pdata->buttons[i];
		struct gpio_button_data *bdata = &ddata->data[i];
		int irq;
		unsigned int type = button->type ?: EV_KEY;

		bdata->input = input;
		bdata->button = button;
		setup_timer(&bdata->timer,
			    gpio_keys_timer, (unsigned long)bdata);
		INIT_WORK(&bdata->work, gpio_keys_report_event);

		irq = imapx200_gpio_to_irq(button->gpio);
		if (irq < 0) {
			error = irq;
			pr_err("gpio-keys: Unable to get irq number"
				" for GPIO %d, error %d\n",
				button->gpio, error);
			goto fail2;
		}
		if(i == 0){
		error = request_irq(irq, gpio_keys_isr,
				    IRQF_DISABLED, 
				    button->desc ? button->desc : "gpio_keys",
				    bdata);
		} else if (i==1) {
		error = request_irq(irq, gpio_menu_keys_isr,
				    IRQF_DISABLED, 
				    button->desc ? button->desc : "gpio_menu_keys",
				    bdata);
		} else if (i==2) {
		error = request_irq(irq, gpio_back_keys_isr,
				    IRQF_DISABLED, 
				    button->desc ? button->desc : "gpio_back_keys",
				    bdata);
		}else {
		}

		if (error) {
			pr_err("gpio-keys: Unable to claim irq %d; error %d\n",
				irq, error);
			goto fail2;
		}

		if (button->wakeup)
			wakeup = 1;

		input_set_capability(input, type, button->code);
	}

	/* Add 116 to keybit, by warits */
	set_bit(KEY_POWER, input->keybit);
	set_bit(KEY_SPACE, input->keybit);
	set_bit(KEY_BACK, input->keybit);
	set_bit(KEY_MENU, input->keybit);
	set_bit(KEY_HOME, input->keybit);
	set_bit(KEY_VOLUMEUP, input->keybit);
	set_bit(KEY_VOLUMEDOWN, input->keybit);
	error = input_register_device(input);
	if (error) {
		pr_err("gpio-keys: Unable to register input device, "
			"error: %d\n", error);
		goto fail2;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

 fail2:
	while (--i >= 0) {
		free_irq(imapx200_gpio_to_irq(pdata->buttons[i].gpio), &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		cancel_work_sync(&ddata->data[i].work);
	}

	platform_set_drvdata(pdev, NULL);
 fail1:
	input_free_device(input);
	kfree(ddata);

	return error;
}

static int __devexit gpio_keys_remove(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	device_init_wakeup(&pdev->dev, 0);

	for (i = 0; i < pdata->nbuttons; i++) {
		int irq = imapx200_gpio_to_irq(pdata->buttons[i].gpio);
		free_irq(irq, &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		cancel_work_sync(&ddata->data[i].work);
	}
	input_unregister_device(input);

	return 0;
}


#ifdef CONFIG_PM
static int gpio_keys_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = imapx200_gpio_to_irq(button->gpio);
				enable_irq_wake(irq);
			}
		}
	}

	return 0;
}

static int gpio_keys_resume(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;
	struct input_dev *input = ddata->data[0].input;
	input_event(input, EV_KEY, 116, 1);
	input_event(input, EV_KEY, 116, 0);
	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = imapx200_gpio_to_irq(button->gpio);
				disable_irq_wake(irq);
			}
		}
	}
	return 0;
}
#else
#define gpio_keys_suspend	NULL
#define gpio_keys_resume	NULL
#endif

static struct platform_driver gpio_keys_device_driver = {
	.probe		= gpio_keys_probe,
	.remove		= __devexit_p(gpio_keys_remove),
	.suspend	= gpio_keys_suspend,
	.resume		= gpio_keys_resume,
	.driver		= {
		.name	= "gpio-keys",
		.owner	= THIS_MODULE,
	}
};

static int __init gpio_keys_init(void)
{
	return platform_driver_register(&gpio_keys_device_driver);
}

static void __exit gpio_keys_exit(void)
{
	platform_driver_unregister(&gpio_keys_device_driver);
}

module_init(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Keyboard driver for CPU GPIOs");
MODULE_ALIAS("platform:gpio-keys");
