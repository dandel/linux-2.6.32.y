#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

#include <asm/io.h>
#include <plat/imapx.h>

struct wifi_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	//struct work_struct *work;
};

struct wifi_switch_data *switch_data;

struct work_struct switch_wifi_work;
EXPORT_SYMBOL(switch_wifi_work);

static void gpio_switch_work(struct work_struct *work)
{
	int state, tmp1, tmp2, tmp3;
//	struct wifi_switch_data	*data =
//		container_of(work, struct wifi_switch_data, work);

//	printk("enter function %s, at line %d \n", __func__, __LINE__);
	tmp1 = readl(rGPEDAT);
//	tmp2 = readl(rGPJDAT);
//	tmp3 = readl(rGPPDAT);
	if (tmp1 & (0x1 << 8))
	{
//		tmp2 |= 0x1 << 4;
//		writel(tmp2, rGPJDAT);
//		tmp3 |= 0x1 << 0;
//		writel(tmp3, rGPPDAT);
		state = 1;
	}
	else
	{
//		tmp2 &= ~(0x1 << 4);
//		writel(tmp2, rGPJDAT); 
//		tmp3 &= ~(0x1 << 0);
//		writel(tmp3, rGPPDAT); 
		state = 0;
	}

	switch_set_state(&switch_data->sdev, state);
}
/*
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct wifi_switch_data *switch_data =
	    (struct wifi_switch_data *)dev_id;

	printk("enter function %s, at line %d \n", __func__, __LINE__);
	if(readl(rEINTG4PEND) & (0x1 << 8))
	{
		writel(0x1 << 8, rEINTG4PEND);
		schedule_work(&switch_wifi_work);
	}
	return IRQ_HANDLED;
}
*/
static ssize_t switch_wifi_print_state(struct switch_dev *sdev, char *buf)
{
	struct wifi_switch_data	*switch_data =
		container_of(sdev, struct wifi_switch_data, sdev);
	const char *state;
	//printk("enter function %s, at line %d \n", __func__, __LINE__);
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int wifi_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
//	struct wifi_switch_data *switch_data;
	int ret = 0, tmp;

	//printk("enter function %s, at line %d \n", __func__, __LINE__);
	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct wifi_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_wifi_print_state;

	printk("switch_gpio is %d\n", switch_data->gpio);
    ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	/* config GPJ4 as output */
	tmp = readl(rGPJCON);
	tmp &= ~(0x3 << 8);
	tmp |= 0x1 << 8;
	writel(tmp, rGPJCON);

	tmp = readl(rGPPCON);
	tmp &= ~(0x3 << 0);
	tmp |= 0x1 << 0;
	writel(tmp, rGPPCON);

	tmp = readl(rGPJDAT);
	tmp |= (0x1 << 4);
	writel(tmp, rGPJDAT);
	tmp = readl(rGPPDAT);
	tmp |= (0x1 << 0);
	writel(tmp, rGPPDAT);

	/* config GPE8 as EINT */
	tmp = readl(rGPECON);
	tmp &= ~(0x3 << 16);
	tmp |= 0x0 << 16;
	writel(tmp, rGPECON);
	
	tmp = readl(rEINTGCON);
	tmp &= ~(0x7 << 12);
	tmp |= 0x7 << 12;
	writel(tmp, rEINTGCON);

	tmp = readl(rEINTG4MASK);
	tmp &= ~(0x1 << 8);
	tmp |= 0x0 << 8;
	writel(tmp, rEINTG4MASK);

	INIT_WORK(&switch_wifi_work, gpio_switch_work);

	switch_data->irq = (int)switch_data->gpio;
//	ret = request_irq(switch_data->irq, gpio_irq_handler,
//	   IRQF_DISABLED, pdev->name, switch_data);
//	if (ret < 0)
//		goto err_request_irq;

	/* Perform initial detection */
	gpio_switch_work(&switch_wifi_work);

	schedule_work(&switch_wifi_work);

	//printk("enter function %s, at line %d \n", __func__, __LINE__);
	return 0;

err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit wifi_switch_remove(struct platform_device *pdev)
{
	struct wifi_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_work_sync(&switch_wifi_work);
    switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);
	return 0;
}

static int wifi_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
	//struct wifi_switch_data *switch_data = platform_get_drvdata(pdev);

	//schedule_work(&switch_wifi_work);
	return 0;
}

static int wifi_switch_resume(struct platform_device *pdev)
{
	//struct wifi_switch_data *switch_data = platform_get_drvdata(pdev);

	schedule_work(&switch_wifi_work);
	return 0;
}

static struct platform_driver wifi_switch_driver = {
	.probe		= wifi_switch_probe,
	.remove		= __devexit_p(wifi_switch_remove),
	.suspend	= wifi_switch_suspend,
	.resume		= wifi_switch_resume,
	.driver		= {
		.name	= "switch-wifi",
		.owner	= THIS_MODULE,
	},
};

static int __init wifi_switch_init(void)
{
	printk("wifi_switch module init\n");
	return platform_driver_register(&wifi_switch_driver);
}

static void __exit wifi_switch_exit(void)
{
	platform_driver_unregister(&wifi_switch_driver);
}

module_init(wifi_switch_init);
//late_initcall(wifi_switch_init);
module_exit(wifi_switch_exit);

MODULE_AUTHOR("Bob.yang <Bob.yang@infotmic.com.cn>");
MODULE_DESCRIPTION("WIFI Switch driver");
MODULE_LICENSE("GPL");
