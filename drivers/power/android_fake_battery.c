/*
 * * Fake Battery driver for android
 * *
 * * Copyright © 2009 Rockie Cheng <aokikyon@gmail.com>
 * *
 * * This program is free software; you can redistribute it and/or modify
 * * it under the terms of the GNU General Public License version 2 as
 * * published by the Free Software Foundation.
 * */
/*
 * the condition that low level voltage warning message can display are:
 * 1, the voltage lower than 15%
 * 2, there is a debounce between two voltage and condition 1 is satisfied.
 **/
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
//#undef CONFIG_IMAP_PRODUCTION
#include <linux/spi/ads7846.h>
#include <mach/imapx_gpio.h>
#include <mach/imapx_base_reg.h>
#include <linux/io.h>
#include <asm/delay.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <mach/imapx_gpio.h>
#define BAT_STAT_PRESENT 0x01
#define BAT_STAT_FULL   0x02
#define BAT_STAT_LOW   0x04
#define BAT_STAT_DESTROY 0x08
#define BAT_STAT_AC   0x10
#define BAT_STAT_CHARGING 0x20
#define BAT_STAT_DISCHARGING 0x40

#define BAT_ERR_INFOFAIL 0x02
#define BAT_ERR_OVERVOLTAGE 0x04
#define BAT_ERR_OVERTEMP 0x05
#define BAT_ERR_GAUGESTOP 0x06
#define BAT_ERR_OUT_OF_CONTROL 0x07
#define BAT_ERR_ID_FAIL   0x09
#define BAT_ERR_ACR_FAIL 0x10

#define BAT_ADDR_MFR_TYPE 0x5F
#define IMAP_ADAPTER 615
#ifdef CONFIG_IMAP_PRODUCTION_P1011A
#define IMAP_BAT_FULL 605
#define IMAP_BAT_CRITICAL 505
#elif CONFIG_IMAP_PRODUCTION_P1011B
#define IMAP_BAT_FULL 580
#define IMAP_BAT_CRITICAL 445
#else
#endif
//#define IMAP_BAT_CRITICAL 548

#define BATT_INITIAL 0
#define BATT_ON 1
static int battery_state=BATT_INITIAL;
static struct timer_list supply_timer;
static int batt, old_batt=580;
static int charging_status=0;

static int ischargingfull(void)
{
	unsigned long tmp;
	tmp = (__raw_readl(rGPODAT)) & 0x100;
	return tmp; 
}
static int android_ac_get_prop(struct power_supply *psy,
   enum power_supply_property psp,
   union power_supply_propval *val)
{

	switch (psp)
	{
		case POWER_SUPPLY_PROP_ONLINE:
			if (charging_status|| (ischargingfull() && old_batt > IMAP_BAT_FULL))
			{
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			}
			else{
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			break;
		default:
			break;
	}
	return 0;
}

static enum power_supply_property android_ac_props[] =
{
	POWER_SUPPLY_PROP_ONLINE,
};

static struct power_supply android_ac =
{
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = android_ac_props,
	.num_properties = ARRAY_SIZE(android_ac_props),
	.get_property = android_ac_get_prop,
};

static int android_bat_get_status(union power_supply_propval *val)
{
	val->intval = POWER_SUPPLY_STATUS_FULL;
#ifdef CONFIG_IMAP_PRODUCTION

	if (charging_status){
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
	}else {
	if (old_batt >= (IMAP_BAT_FULL)) {
			val->intval = POWER_SUPPLY_STATUS_FULL; //ac
	}else {
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING; 
	}
	}

#endif
	return 0;
}

static int android_bat_get_capacity(union power_supply_propval *val)
{
	volatile int tmp_batt;
	tmp_batt = old_batt;
	//printk("tmp_batt is %d\n", tmp_batt);
	/*
	 * voltage 	mapping
	 * 29%          19%
	 * .       	.
	 * .		.
	 * .		.
	 * 21%		11%
	 * <=20%		0%        
	 */
	val->intval = 100;
#ifdef CONFIG_IMAP_PRODUCTION

	if (charging_status){
		val->intval = 30;
	}
	else {
		if (tmp_batt <= IMAP_BAT_CRITICAL+20){
			val->intval = 0;
		}
		else if  (tmp_batt < IMAP_BAT_CRITICAL+30 && tmp_batt > IMAP_BAT_CRITICAL+20){
			tmp_batt = tmp_batt - 10;
			val->intval = (100*(tmp_batt - IMAP_BAT_CRITICAL))/(IMAP_BAT_FULL - IMAP_BAT_CRITICAL);
		}
		else{
		val->intval = (100*(tmp_batt - IMAP_BAT_CRITICAL))/(IMAP_BAT_FULL - IMAP_BAT_CRITICAL);
		}
		if (val->intval >100 )
			val->intval = 100;
	}

#endif
	//printk("*****val->intval is %d\n", val->intval);
	return 0;
}


static int android_bat_get_health(union power_supply_propval *val)
{

	val->intval = POWER_SUPPLY_HEALTH_GOOD;
	return 0;
}

static int android_bat_get_mfr(union power_supply_propval *val)
{

	val->strval = "Rockie";
	return 0;
}

static int android_bat_get_tech(union power_supply_propval *val)
{
	val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
	return 0;
}

static int android_bat_get_property(struct power_supply *psy,
   enum power_supply_property psp,
   union power_supply_propval *val)
{
	int ret = 0;

	switch (psp)
	{
		case POWER_SUPPLY_PROP_STATUS:

			ret = android_bat_get_status(val);
			if (ret)
			  return ret;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = BAT_STAT_PRESENT;
			break;

		case POWER_SUPPLY_PROP_HEALTH:
			ret = android_bat_get_health(val);
			break;
		case POWER_SUPPLY_PROP_MANUFACTURER:
			ret = android_bat_get_mfr(val);
			if (ret)
			  return ret;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			ret = android_bat_get_tech(val);
			if (ret)
			  return ret;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_AVG:
			val->intval = 3;
			break;
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			val->intval = 3;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			//val->intval = 100;
			android_bat_get_capacity(val);
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = 50;
			break;
		case POWER_SUPPLY_PROP_TEMP_AMBIENT:
			val->intval = 50;
			break;
		case POWER_SUPPLY_PROP_CHARGE_COUNTER:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_SERIAL_NUMBER:
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

static enum power_supply_property android_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
};

/*********************************************************************
 * *   Initialisation
 * *********************************************************************/

static struct platform_device *bat_pdev;

static struct power_supply android_bat =
{
	.properties = android_bat_props,
	.num_properties = ARRAY_SIZE(android_bat_props),
	.get_property = android_bat_get_property,
	.use_for_apm = 1,
};
#ifdef CONFIG_IMAP_PRODUCTION
static void supply_timer_func(unsigned long unused)
{	
	batt = battery_val;
	if (!ischargingfull()){//charging
		old_batt = batt;
		power_supply_changed(&android_bat);
		charging_status = 1;
	}
	else if(old_batt > batt && battery_state == BATT_INITIAL){//initializing
		old_batt = batt;
		power_supply_changed(&android_bat);
		charging_status = 0;
		battery_state = BATT_ON;
	} 
	else if((old_batt - batt < 3) && (old_batt - batt >0)) {//using battery
	old_batt = batt;
	power_supply_changed(&android_bat);
	charging_status = 0;
	}else if(old_batt > IMAP_ADAPTER && batt < IMAP_ADAPTER){//adaptor disconnect 
		old_batt = batt;
		power_supply_changed(&android_bat);
		charging_status = 0;

	}
	else {

	}
	mod_timer(&supply_timer,\
		  jiffies + msecs_to_jiffies(2000));
}
#endif
static int __init android_bat_init(void)
{
	int ret = 0;
	unsigned long tmp;
	/**********************************************/
	//here, we need to know the power type, AC or Battery and detect the voltage each 5 second.
	//need to register a irq for pluging in or out the AC.
	//at last, need to monitor the voltage each 5 second.
	//now, we have a question, cannot display the AC icon.
	/***************************************************/
/*******************************************************/
#ifdef CONFIG_IMAP_PRODUCTION
	tmp = __raw_readl(rGPOCON);
	tmp &= ~(0x3<<16);
	__raw_writel(tmp, rGPOCON);
	tmp = __raw_readl(rGPOPUD);
	tmp &= ~(0x1<<8);
	__raw_writel(tmp, rGPOPUD);

#ifdef CONFIG_IMAP_PRODUCTION_P1011A
	tmp = __raw_readl(rGPHCON);
	tmp &= ~(0x3<<4);
	__raw_writel(tmp, rGPHCON);
	tmp = __raw_readl(rGPHPUD);
	tmp |= (0x1<<2);
	__raw_writel(tmp, rGPHPUD);
#elif CONFIG_IMAP_PRODUCTION_P1011B
	tmp = __raw_readl(rGPBCON);
	tmp &= ~(0x3<<2);
	__raw_writel(tmp, rGPBCON);

#elif CONFIG_IMAP_PRODUCTION_P1011C
#else
#endif
	setup_timer(&supply_timer, supply_timer_func, 0);
	mod_timer(&supply_timer,\
		  jiffies + msecs_to_jiffies(3000));
#endif
	/*******************************/
	bat_pdev = platform_device_register_simple("battery", 0, NULL, 0);

	ret = power_supply_register(&bat_pdev->dev, &android_ac);
	if (ret)
	  goto ac_failed;

	android_bat.name = bat_pdev->name;

	ret = power_supply_register(&bat_pdev->dev, &android_bat);
	if (ret)
	  goto battery_failed;

	goto success;

	power_supply_unregister(&android_bat);
battery_failed:
	power_supply_unregister(&android_ac);
ac_failed:
	platform_device_unregister(bat_pdev);
success:
	return ret;
}

static void __exit android_bat_exit(void)
{
	power_supply_unregister(&android_bat);
	power_supply_unregister(&android_ac);
	platform_device_unregister(bat_pdev);
}

module_init(android_bat_init);
module_exit(android_bat_exit);

MODULE_AUTHOR("Rockie Cheng <aokikyon@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fake Battery driver for android");
