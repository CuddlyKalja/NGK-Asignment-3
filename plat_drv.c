#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/timer.h>

#define DRIVER_NAME "plat_drv"

static struct class *gpio_class;
static struct gpio_desc *my_gpio;
static struct timer_list toggle_timer;
static int toggle_state = 0;
static unsigned long toggle_delay = 1000; // milliseconds

static void toggle_gpio(struct timer_list *t)
{
    static bool state = false;

    state = !state;
    gpiod_set_value(my_gpio, state);
    mod_timer(&toggle_timer, jiffies + msecs_to_jiffies(toggle_delay));
}

static ssize_t gpio_toggle_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", toggle_state);
}

static ssize_t gpio_toggle_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long val;

    if (kstrtoul(buf, 10, &val) != 0)
        return -EINVAL;

    toggle_state = val;

    if (toggle_state) {
        mod_timer(&toggle_timer, jiffies + msecs_to_jiffies(toggle_delay));
        pr_info("Toggle started with delay %lu ms\n", toggle_delay);
    } else {
        del_timer_sync(&toggle_timer);
        pr_info("Toggle stopped\n");
    }

    return size;
}

static ssize_t gpio_toggle_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%lu\n", toggle_delay);
}

static ssize_t gpio_toggle_delay_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long val;

    if (kstrtoul(buf, 10, &val) != 0)
        return -EINVAL;

    toggle_delay = val;
    pr_info("Delay set to %lu ms\n", toggle_delay);
    return size;
}

static DEVICE_ATTR_RW(gpio_toggle_state);
static DEVICE_ATTR_RW(gpio_toggle_delay);

static struct attribute *gpio_attrs[] = {
    &dev_attr_gpio_toggle_state.attr,
    &dev_attr_gpio_toggle_delay.attr,
    NULL,
};

ATTRIBUTE_GROUPS(gpio);

static int plat_drv_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    my_gpio = gpiod_get(dev, NULL, GPIOD_OUT_LOW);
    if (IS_ERR(my_gpio)) {
        dev_err(dev, "Failed to get GPIO\n");
        return PTR_ERR(my_gpio);
    }

    device_create(gpio_class, NULL, MKDEV(0, 0), NULL, "platgpio");

    timer_setup(&toggle_timer, toggle_gpio, 0);
    pr_info("plat_drv: >>> probe() called\n");

    return 0;
}

static int plat_drv_remove(struct platform_device *pdev)
{
    del_timer_sync(&toggle_timer);
    gpiod_put(my_gpio);
    device_destroy(gpio_class, MKDEV(0, 0));
    pr_info("Driver removed\n");
    return 0;
}

static const struct of_device_id plat_drv_of_match[] = {
    { .compatible = "au-ece,plat_drv", },
    {},
};
MODULE_DEVICE_TABLE(of, plat_drv_of_match);

static struct platform_driver plat_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = plat_drv_of_match,
    },
    .probe = plat_drv_probe,
    .remove = plat_drv_remove,
};

static int __init plat_drv_init(void)
{
    gpio_class = class_create(THIS_MODULE, DRIVER_NAME);
    gpio_class->dev_groups = gpio_groups;

    return platform_driver_register(&plat_driver);
}

static void __exit plat_drv_exit(void)
{
    platform_driver_unregister(&plat_driver);
    class_destroy(gpio_class);
}

module_init(plat_drv_init);
module_exit(plat_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tam & Kalja");
MODULE_DESCRIPTION("Platform driver using gpiod and timer");
