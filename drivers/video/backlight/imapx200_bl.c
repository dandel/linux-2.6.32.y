/************************************************************
**   driver/video/backlight/imapx200_bl.c
**  
**   Copyright (c) 2009~2014 ShangHai Infotm .Ltd all rights reserved.
**
**   Use of infoTM's code  is governed by terms and conditions
**   stated in the accompanying licensing statment.
**   
**   Description: backlight control driver for imapx200 SOC
** 
**
**   AUTHOR:
**   Haixu Fu	 <haixu_fu@infotm.com>
**   warits		 <warits.wang@infotm.com>
**
**   
**   Revision History:
**  ---------------------------------
**   1.1  03/06/2010   Haixu Fu
**   1.2  04/01/2010   warits (add android features)
*******************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/leds.h>
#include <linux/sysdev.h>

#define	IMAP_MAX_INTENSITY (0xff)
#define	IMAP_DEFAULT_INTENSITY IMAP_MAX_INTENSITY

static int imapbl_suspended = 0;
static int imapbl_cur_brightness = IMAP_DEFAULT_INTENSITY;

struct imapbl_data {
	int current_intensity;
	int suspend;
};

static DEFINE_MUTEX(bl_mutex);

extern int imap_timer_setup(int channel,unsigned long g_tcnt,unsigned long gtcmp);
extern int imap_pwm_suspend(struct sys_device *pdev, pm_message_t pm);
extern int imap_pwm_resume(struct sys_device *pdev);

static void imapbl_write_intensity(int intensity)
{
	int g_tcmp;
#if defined(CONFIG_FB_IMAP_LCD800X600)
	int g_tcnt = 0xff;
	g_tcmp = (intensity *(0x2ff - 0x20))/0xff + 0x20;
	if (g_tcmp > 0x2ff)
		g_tcmp = 0x2ff;
	else if (g_tcmp <= 0x20)
		g_tcmp = 0;
	else{
	}

	imap_timer_setup(2,0x2ff,g_tcmp);
#elif defined(CONFIG_FB_IMAP_LCD1024X600)
/*****************************************/
	g_tcmp = intensity *3;
	if (intensity <100)
	       g_tcmp = intensity;	
	if (intensity < 30)
		g_tcmp = 0x20;
	/******************************/
	imap_timer_setup(2,0x2ff,g_tcmp);
#endif

	return;
}
	
static void imapbl_send_intensity(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;
	struct imapbl_data *devdata = dev_get_drvdata(&bd->dev);

	if(bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if(bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;
	if(imapbl_suspended)
		intensity = 0;
	mutex_lock(&bl_mutex);
	imapbl_write_intensity(intensity);
	mutex_unlock(&bl_mutex);
	devdata->current_intensity = intensity ;
}

static int imapbl_set_intensity(struct backlight_device *bd)
{
	imapbl_send_intensity(bd);
	printk(KERN_INFO "BRIGHT SET OK\n");
	return 0;
}


/* set and get funcs for ANDROID */
static void imapbl_leds_set_brightness(struct led_classdev *led_cdev,
   int brightness)
{

//	printk(KERN_INFO "Calling with brightness %d\n", brightness);
	mutex_lock(&bl_mutex);
	imapbl_write_intensity(brightness);
	imapbl_cur_brightness = brightness;
	mutex_unlock(&bl_mutex);
}   

static int imapbl_leds_get_brightness(struct led_classdev *led_cdev)
{
	return imapbl_cur_brightness;
}

static int imapbl_get_intensity(struct backlight_device *bd)
{
	struct imapbl_data *devdata = dev_get_drvdata(&bd->dev);
	return devdata->current_intensity;
}

static struct backlight_ops imapbl_ops = {
	.get_brightness = imapbl_get_intensity,
	.update_status 	= imapbl_set_intensity,
};

#ifdef CONFIG_PM

static int imapbl_suspend(struct platform_device *pdev,pm_message_t state)
{
	/* XXX ANDROID XXX */
#if 0 
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct imapbl_data *devdata = dev_get_drvdata(&bd->dev);

	devdata->suspend = 1;
	imapbl_send_intensity(bd);
#endif
	imap_pwm_suspend(NULL, state);
	return 0;
}

static int imapbl_resume(struct platform_device *pdev)
{
	/* XXX ANDROID XXX */
#if 0 
	struct	backlight_device *bd = platform_get_drvdata(pdev);
	struct imapbl_data *devdata = dev_get_drvdata(&bd->dev);

	devdata->suspend = 0;
	imapbl_send_intensity(bd);
#endif
	unsigned int temp;
	imap_pwm_resume(NULL);

	temp = __raw_readl(rGPFCON);
	temp &= ~(0x3<<16);
	temp |= 0x2<<16;
	__raw_writel(temp, rGPFCON);

	return 0;
}

#else

#define  imapbl_suspend NULL
#define	 imapbl_resume  NULL

#endif
static struct led_classdev imap_bl_cdev = {
	.name = "lcd-backlight",
	.brightness_set = imapbl_leds_set_brightness,
	.brightness_get = imapbl_leds_get_brightness,
};

static  int imapbl_probe(struct platform_device *pdev)
{
	int error;
	struct backlight_device *bd;
	struct imapbl_data *devdata;
	uint32_t gpf8;
	devdata = kzalloc(sizeof(struct imapbl_data),GFP_KERNEL);
	if(!devdata)
		return -ENOMEM;

	/* Config GPF8 */
#ifdef CONFIG_IMAP_PRODUCTION_P1011A	
	gpf8 = readl(rGPFCON);
	gpf8 &= ~(0x3 << 16);
	gpf8 |= (0x2 << 16);
	writel(gpf8, rGPFCON);
#elif CONFIG_IMAP_PRODUCTION_P1011B
	gpf8 = readl(rGPFCON);
	gpf8 &= ~(0x3 << 16);
	gpf8 |= (0x2 << 16);
	writel(gpf8, rGPFCON);
#elif CONFIG_IMAP_PRODUCTION_P1011C
#elif CONFIG_IMAP_PRODUCTION_P0811B
	gpf8 = readl(rGPFCON);
	gpf8 &= ~(0x3 << 16);
	gpf8 |= (0x2 << 16);
	writel(gpf8, rGPFCON);
#endif

	bd = backlight_device_register ("lcd-backlight",&pdev->dev,devdata,
			&imapbl_ops);

	if(IS_ERR(bd))
		return PTR_ERR(bd);

	/* Register LEDS for ANDROID GUI backlight control */
	/**                                                                                           
	 * led_classdev_register - register a new object of led_classdev class.                       
	 * @parent: The device to register.                                                           
	 * @led_cdev: the led_classdev structure for this device.                                     
	 */                                                                                           
	error = led_classdev_register(&pdev->dev, &imap_bl_cdev);
	if(error)
	{
		printk(KERN_INFO "Registe leds lcd-backlight failed.\n");
	}

	platform_set_drvdata(pdev,bd);
	bd->props.max_brightness = IMAP_MAX_INTENSITY;
	bd->props.brightness 	 = IMAP_DEFAULT_INTENSITY;
	imapbl_send_intensity(bd);
	
	return 0;
}

static int imapbl_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct imapbl_data *devdata = dev_get_drvdata(&bd->dev);
	
	kfree(devdata);
	bd->props.brightness = 0;
	bd->props.power	= 0;
	imapbl_send_intensity(bd);
	backlight_device_unregister(bd);

	return 0;
}

static struct platform_driver imapbl_driver = {
	.probe		= imapbl_probe,
	.remove		= imapbl_remove,
	.suspend	= imapbl_suspend,
	.resume		= imapbl_resume,
	.driver		= {
		.name	= "imapx200-lcd-backlights",
		.owner  = THIS_MODULE,
	},
};

static int __init imapbl_init(void)
{
	int ret;
	ret = platform_driver_register(&imapbl_driver);
	if(ret)
		return ret;
	return 0;
}

static void __exit imapbl_exit(void)
{
	platform_driver_unregister(&imapbl_driver);
}



module_init(imapbl_init);
module_exit(imapbl_exit);

MODULE_AUTHOR("ronaldo, warits");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IMAPX200 Backlight Driver");

