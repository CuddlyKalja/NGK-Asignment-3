#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>

#define DRIVER_NAME "gpio_class"
#define MAX_GPIOS   10

static dev_t dev;
static struct class *gpio_class;
static struct cdev led_cdev;
static int major;

static struct timer_list toggle_timer;
static int toggle_state = 0;
static unsigned long toggle_delay = 1000; // ms

struct gpio_dev {
    int no;
    int dir; // 0=input, 1=output
};

static struct gpio_dev gpio_devs[MAX_GPIOS];
static int gpios_len;

// Timer toggle logic
static void toggle_gpio(struct timer_list *t)
{
    static bool level = false;
    level = !level;

    for (int i = 0; i < gpios_len; i++) {
        if (gpio_devs[i].dir == 1) {
            gpio_set_value(gpio_devs[i].no, level);
        }
    }
    mod_timer(&toggle_timer, jiffies + msecs_to_jiffies(toggle_delay));
}

static ssize_t gpio_toggle_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", toggle_state);
}

static ssize_t gpio_toggle_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long val;
    if (kstrtoul(buf, 10, &val)) return -EINVAL;
    toggle_state = val;

    if (toggle_state) {
        mod_timer(&toggle_timer, jiffies + msecs_to_jiffies(toggle_delay));
        pr_info("[GPIO] Toggle started\n");
    } else {
        del_timer_sync(&toggle_timer);
        pr_info("[GPIO] Toggle stopped\n");
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
    if (kstrtoul(buf, 10, &val)) return -EINVAL;
    toggle_delay = val;
    pr_info("[GPIO] Delay set to %lu ms\n", toggle_delay);
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

// Platform driver probe
static int plat_drv_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    int gpio, ret, i;
    dev_t devno;

    gpios_len = of_gpio_count(np);
    if (gpios_len <= 0 || gpios_len > MAX_GPIOS) return -EINVAL;

    for (i = 0; i < gpios_len; i++) {
        gpio = of_get_gpio(np, i);
        if (gpio < 0) return gpio;

        gpio_devs[i].no = gpio;
        if (of_property_read_u32_index(np, "gpio-directions", i, &gpio_devs[i].dir))
            gpio_devs[i].dir = 0; // default input

        ret = gpio_request(gpio, "my_gpio");
        if (ret) return ret;

        if (gpio_devs[i].dir == 0)
            gpio_direction_input(gpio);
        else
            gpio_direction_output(gpio, 0);

        devno = MKDEV(major, i);
        device_create(gpio_class, NULL, devno, NULL, "gpio_dev%d", i);
        device_create(gpio_class, NULL, MKDEV(major, MAX_GPIOS), NULL, "platgpio");
    }

    timer_setup(&toggle_timer, toggle_gpio, 0);
    pr_info("GPIO driver probed\n");
    return 0;
}

static int plat_drv_remove(struct platform_device *pdev)
{
    int i;
    del_timer_sync(&toggle_timer);

    for (i = 0; i < gpios_len; i++) {
        device_destroy(gpio_class, MKDEV(major, i));
        gpio_free(gpio_devs[i].no);
    }
    class_destroy(gpio_class);
    return 0;
}

// Char device ops
static int plat_drv_open(struct inode *inode, struct file *filep) {
    return 0;
}
static int plat_drv_release(struct inode *inode, struct file *filep) {
    return 0;
}
static ssize_t plat_drv_read(struct file *filep, char __user *ubuf, size_t count, loff_t *f_pos) {
    int minor = iminor(filep->f_inode);
    char buf[8];
    int val = gpio_get_value(gpio_devs[minor].no);
    int len = snprintf(buf, sizeof(buf), "%d\n", val);
    return simple_read_from_buffer(ubuf, count, f_pos, buf, len);
}
static ssize_t plat_drv_write(struct file *filep, const char __user *ubuf, size_t count, loff_t *f_pos) {
    int minor = iminor(filep->f_inode);
    char buf[8];
    long val;
    if (gpio_devs[minor].dir != 1) return -EINVAL;
    if (copy_from_user(buf, ubuf, min(count, sizeof(buf) - 1))) return -EFAULT;
    buf[min(count, sizeof(buf) - 1)] = '\0';
    if (kstrtol(buf, 10, &val)) return -EINVAL;
    gpio_set_value(gpio_devs[minor].no, val ? 1 : 0);
    return count;
}

static struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = plat_drv_open,
    .release = plat_drv_release,
    .read    = plat_drv_read,
    .write   = plat_drv_write,
};

static const struct of_device_id plat_drv_of_match[] = {
    { .compatible = "au-ece,plat_drv" },
    {}
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
    int ret;
    ret = alloc_chrdev_region(&dev, 0, MAX_GPIOS, DRIVER_NAME);
    if (ret) return ret;
    major = MAJOR(dev);

    gpio_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(gpio_class)) return PTR_ERR(gpio_class);
    gpio_class->dev_groups = gpio_groups;

    cdev_init(&led_cdev, &led_fops);
    cdev_add(&led_cdev, dev, MAX_GPIOS);
    return platform_driver_register(&plat_driver);
}

static void __exit plat_drv_exit(void)
{
    cdev_del(&led_cdev);
    unregister_chrdev_region(dev, MAX_GPIOS);
    platform_driver_unregister(&plat_driver);
    class_destroy(gpio_class);
}

module_init(plat_drv_init);
module_exit(plat_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tam & Kalja");
MODULE_DESCRIPTION("Combined GPIO char driver + toggle sysfs");
