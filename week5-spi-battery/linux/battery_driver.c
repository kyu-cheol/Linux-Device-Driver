#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/spi/spi.h> // SPI 관렦 API를 사용하기 위해 include 함
#include <linux/reboot.h>

#define COMENTO_SSI_GET_BATTERY 1        // instruction code : get current battery

struct battery_spi_device {
	struct spi_device *dev;
	struct spi_message msg;
	struct spi_transfer xfer[2];
	char tx_buf[1];
	char rx_buf[1];
	struct mutex lock;
	struct timer_list battery_timer;
	struct work_struct poweroff_work;
};

static ssize_t comento_show_battery(struct device *dev, struct device_attribute *attr, char *buf) {
	ssize_t ret;
	uint8_t val;
	struct battery_spi_device *bsdev = dev_get_drvdata(dev);

	mutex_lock(&bsdev->lock);
	bsdev->tx_buf[0] = COMENTO_SSI_GET_BATTERY;

	ret = spi_sync(bsdev->dev, &bsdev->msg);
	if (ret < 0) {
		printk(KERN_ERR "comento_spi: spi_sync failed - %d\n", ret);
		mutex_unlock(&bsdev->lock);
		return ret;
	}

	val = bsdev->rx_buf[0];
	mutex_unlock(&bsdev->lock);

	return sprintf(buf, "%d\n", val);
}

DEVICE_ATTR(percent, 0444, comento_show_battery, NULL);
static struct attribute *battery_spi_attributes[] = {
	&dev_attr_percent.attr,
	NULL,
};

static const struct attribute_group battery_spi_attr_group = {
	.attrs = battery_spi_attributes,
};

static void battery_lack_poweroff_work_handler(struct work_struct *work){
	struct battery_spi_device *bsdev = container_of(work, struct battery_spi_device, poweroff_work);
	uint8_t val;

	mutex_lock(&bsdev->lock);
	bsdev->tx_buf[0] = COMENTO_SSI_GET_BATTERY;

	spi_sync(bsdev->dev, &bsdev->msg);
	
	val = bsdev->rx_buf[0];
	mutex_unlock(&bsdev->lock);

	if (val < 5) {
		kernel_power_off();
	}
}

static void timer_callback(struct timer_list *timer) {
	struct battery_spi_device *bsdev = container_of(timer, struct battery_spi_device, battery_timer);
	
	schedule_work(&bsdev->poweroff_work);
	mod_timer(&bsdev->battery_timer, jiffies + msecs_to_jiffies(2000));
}

static int battery_spi_probe(struct spi_device *dev) {
	struct battery_spi_device *bsdev;
	int err;

	dev->bits_per_word = 8;
	dev->mode = SPI_MODE_0;

	err = spi_setup(dev);
	if (err < 0)
		return err;

	bsdev = devm_kzalloc(&dev->dev, sizeof(struct battery_spi_device), GFP_KERNEL);
	if (bsdev == NULL)
		return -ENOMEM;

	spi_message_init(&bsdev->msg);

	bsdev->xfer[0].tx_buf = bsdev->tx_buf;
	bsdev->xfer[0].len = 1;
	spi_message_add_tail(&bsdev->xfer[0], &bsdev->msg);

	bsdev->xfer[1].rx_buf = bsdev->rx_buf;
	bsdev->xfer[1].len = 1;
	spi_message_add_tail(&bsdev->xfer[1], &bsdev->msg);

	mutex_init(&bsdev->lock);

	bsdev->dev = dev;

	spi_set_drvdata(dev, bsdev);

	sysfs_create_group(&dev->dev.kobj, &battery_spi_attr_group);

	INIT_WORK(&bsdev->poweroff_work, battery_lack_poweroff_work_handler);
	timer_setup(&bsdev->battery_timer, timer_callback, 0);
	mod_timer(&bsdev->battery_timer, jiffies + msecs_to_jiffies(2000));
	
	return 0;
}

static void battery_spi_remove(struct spi_device *dev) {
	struct battery_spi_device *bsdev = spi_get_drvdata(dev);

	mutex_destroy(&bsdev->lock);
	sysfs_remove_group(&dev->dev.kobj, &battery_spi_attr_group);
}

static const struct spi_device_id battery_spi_ids[] = {
	{ "battery-spi", 0 },
	{ },
};

MODULE_DEVICE_TABLE(spi, battery_spi_ids);

static struct spi_driver battery_spi_driver = {
	.driver = {
		.name = "battery-spi",
	},
	.id_table = battery_spi_ids,
	.probe = battery_spi_probe,
	.remove = battery_spi_remove,
};

module_spi_driver(battery_spi_driver);

MODULE_AUTHOR("KuChul<ck08107@gmail.com>");
MODULE_DESCRIPTION("Battery SPI device Driver");
MODULE_LICENSE("GPL");
