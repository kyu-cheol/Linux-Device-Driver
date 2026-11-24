#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/hashtable.h>
#include <linux/pm_wakeirq.h>
#include <linux/amba/bus.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/gpio/consumer.h>

#define LOGGING_DR 0x00     /* Data register */
#define LOGGING_FR 0x04     /* Flag register */
#define LOGGING_RIS 0x08    /* Raw Interrupt Status Register */
#define LOGGING_ICR 0x0c    /* Interrupt Clear Register */
#define LOGGING_IMSC 0x10   /* Interrupt Mask Set/Clear Register */
#define LOGGING_MIS 0x14    /* Masked Interrupt Status Register */
#define LOGGING_RR 0x18     /* Remove Register */

//#define LOGGING_FLAG_TXFE (1 << 3)
//#define LOGGING_FLAG_TXFF (1 << 2)
//#define COMENTO_FLAG_RXFE (1 << 1)
//#define COMENTO_FLAG_RXFF (1 << 0)

//#define LOGGING_INT_TX (1 << 1)
//#define COMENTO_INT_RX (1 << 0)

#define LOGGING_RM (1 << 0)

//#define LOGGING_FIFO_SIZE 32
#define LOGGING_DMA_BUFFER_SIZE 4096

#define MIN(a,b) ((a)<(b) ? (a):(b))

#define LOGGING_DEV_MAGIC 'L'
#define LOGGING_DEV_IOCTL_CLEAR _IO(LOGGING_DEV_MAGIC, 0)

static int comento_logging_probe(struct amba_device *adev, const struct amba_id *id);
static void comento_logging_remove(struct amba_device *adev);
static irqreturn_t logging_button_interrupt(int irq, void *dev_id);
//static irqreturn_t comento_logging_interrupt(int irq, void *dev_id);
//static void comento_logging_rx_work_handler(struct work_struct *work);
static ssize_t comento_logging_write(struct file *fp, const char __user *buf, size_t len, loff_t *ppos);
//static ssize_t comento_mmio_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos);
static long comento_logging_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);

static const struct amba_id comento_logging_ids[] = {
	{
		.id = 0x00000002,
		.mask = 0x000fffff,
		.data = NULL,
	},
	{0, 0},
};

MODULE_DEVICE_TABLE(amba, comento_logging_ids);

static struct amba_driver comento_logging_driver = {
	.drv = {
		.name = "logging-comento",
	},
	.id_table = comento_logging_ids,
	.probe = comento_logging_probe,
	.remove = comento_logging_remove,
};

module_amba_driver(comento_logging_driver);

struct logging_dma_buf {
	struct dma_chan *chan;
	struct scatterlist sg;
	dma_cookie_t cookie;
	char *buf;
	bool queued;
};

struct comento_logging_device {
	struct device *dev;
	void __iomem *base;
	rwlock_t lock;
	unsigned int minor;
	struct hlist_node hash;
	struct logging_dma_buf tx;
	wait_queue_head_t tx_wq;
	struct gpio_desc *button;
	//struct work_struct rx_work;
	//char rx_buf[32];
	//int rx_size;
};

static unsigned int comento_logging_major;
static unsigned int comento_logging_minor;
static struct class *comento_logging_class;

static DEFINE_HASHTABLE(comento_logging_table, 4);

static struct comento_logging_device *comento_logging_get_device(unsigned int minor) {
	struct comento_logging_device *node;
	
	hash_for_each_possible(comento_logging_table, node, hash, minor) {
		if (node->minor == minor) {
			return node;
		}
	}

	return NULL;
}

static struct file_operations comento_logging_fops = {
	.write = comento_logging_write,
	.unlocked_ioctl = comento_logging_ioctl,
};

static int logging_dma_init(struct device *dev, const char *name, struct dma_slave_config *conf, \
			    struct logging_dma_buf *dma)
{
	int ret;

	dma->chan = dma_request_chan(dev, name);
	if (IS_ERR(dma->chan)) {
		ret = PTR_ERR(dma->chan);
		goto err_req;
	}
	printk(KERN_INFO "Logging Dev: DMA%s%s\n", name, dma_chan_name(dma->chan));

	dmaengine_slave_config(dma->chan, conf);

	dma->buf = devm_kmalloc(dev, LOGGING_DMA_BUFFER_SIZE, GFP_KERNEL | GFP_DMA);
	sg_init_one(&dma->sg, dma->buf, LOGGING_DMA_BUFFER_SIZE);

	dma->queued = false;

	return 0;

 err_req:
	return ret;
}

static int logging_dma_probe(struct amba_device *adev, struct comento_logging_device *cmdev) {
	int ret;

	struct dma_slave_config tx_conf = {
		.dst_addr = adev->res.start + LOGGING_DR,
		.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE,
		.direction = DMA_MEM_TO_DEV,
		.dst_maxburst = 1,
		.device_fc = false,
	};

	ret = logging_dma_init(&adev->dev, "tx", &tx_conf, &cmdev->tx);
	if (ret) {
		printk(KERN_ERR "Logging Dev: unable to initialize DMA tx\n");
		return ret;
	}

	return 0;
}
	
static int comento_logging_probe(struct amba_device *adev, const struct amba_id *id) {
	int ret, irq;
	struct comento_logging_device *cmdev;

	ret = amba_request_regions(adev, NULL);
	if (ret) {
		goto err_req;
	}

	if (!comento_logging_major) {
		ret = register_chrdev(0, "logging_comento", &comento_logging_fops);
		if (ret < 0) {
			goto out;
		}
		comento_logging_major = ret;
	}

	if (!comento_logging_class) {
		comento_logging_class = class_create("logging");
		if (IS_ERR(comento_logging_class)) {
			printk(KERN_INFO "DEVICE CLASS ERROR.\n");
			ret = PTR_ERR(comento_logging_class);
			comento_logging_class = 0;
			goto out;
		}
	}

	cmdev = devm_kzalloc(&adev->dev, sizeof(struct comento_logging_device), GFP_KERNEL);
	if (!cmdev) {
		ret = -ENOMEM;
		goto out;
	}

	cmdev->base = devm_ioremap(&adev->dev, adev->res.start, resource_size(&adev->res));
	if (!cmdev->base) {
		ret = -ENOMEM;
		goto out;
	}

	cmdev->dev = device_create(comento_logging_class, &adev->dev, \
				   MKDEV(comento_logging_major, comento_logging_minor), \
				   NULL, "logging-dev%d", comento_logging_minor);
	if (IS_ERR(cmdev->dev)) {
		printk(KERN_INFO "DEVICE CREATE ERROR.\n");
		ret = PTR_ERR(cmdev->dev);
		cmdev->dev = NULL;
		goto out;
	}

	cmdev->minor = comento_logging_minor++;
	hash_add(comento_logging_table, &cmdev->hash, cmdev->minor);
	rwlock_init(&cmdev->lock);

	cmdev->button = devm_gpiod_get(&adev->dev, "rmbutton", GPIOD_IN);
	irq = gpiod_to_irq(cmdev->button);
	
	printk(KERN_INFO "Comento LOGGING_dEV device driver\n");

	amba_set_drvdata(adev, cmdev);
	device_init_wakeup(&adev->dev, true);
	
	ret = logging_dma_probe(adev, cmdev);
	if (ret) {
		goto out;
	}
	
	init_waitqueue_head(&cmdev->tx_wq);                        // init waiting queue
	//INIT_WORK(&cmdev->rx_work, comento_mmio_rx_work_handler);  // init work queue
	//writel(LOGGING_INT_TX, cmdev->base + LOGGING_IMSC);

	// regist interrupt service routine	
	ret = devm_request_irq(&adev->dev, irq, logging_button_interrupt, IRQF_TRIGGER_RISING, \
			       "logging-comento-gpio", cmdev);
	if (ret) {
		goto out;
	}

	dev_pm_set_wake_irq(&adev->dev, irq);
	/*
	ret = devm_request_irq(&adev->dev, adev->irq[0], comento_logging_interrupt, 0, "logging-comento", cmdev);
	if (ret) {
		goto out;
	}
	
	dev_pm_set_wake_irq(&adev->dev, adev->irq[0]);
	*/
	return 0;
 out:
	amba_release_regions(adev);
err_req:
	return ret;
}

static void comento_logging_remove(struct amba_device *adev) {
	struct comento_logging_device *cmdev = dev_get_drvdata(&adev->dev);

	if (cmdev->dev) {
		device_destroy(comento_logging_class, cmdev->dev->devt);
		hash_del(&cmdev->hash);

		device_init_wakeup(&adev->dev, false);
		dev_pm_clear_wake_irq(&adev->dev);
		//dev_pm_clear_wake_irq(&adev->dev);
		dma_release_channel(cmdev->tx.chan);
	}

	amba_release_regions(adev);
}

static void logging_dma_tx_callback(void *data)
{
	struct comento_logging_device *cmdev = (struct comento_logging_device *)data;
	// 스캐터 리스트 주소공갂에서 제거
	dma_unmap_sg(cmdev->dev->parent, &cmdev->tx.sg, 1, DMA_TO_DEVICE);
	// DMA 동작 중이 아님으로 변경
	cmdev->tx.queued = false;
	// DMA가 동작 중 아님으로 바뀌길 원하는 대기 큐 깨우기
	wake_up_interruptible(&cmdev->tx_wq);
}

static ssize_t comento_logging_write(struct file *fp, const char __user *buf, size_t len, loff_t *ppos) {
	struct comento_logging_device *cmdev;
	ssize_t written_bytes = 0, bytes;
	struct dma_async_tx_descriptor *desc;
	
	cmdev = comento_logging_get_device(iminor(fp->f_inode));
	//write_lock(&cmdev->lock);

	while (len > 0) {
		wait_event_interruptible(cmdev->tx_wq, !cmdev->tx.queued);
		bytes = MIN(len, LOGGING_DMA_BUFFER_SIZE);
		bytes -= copy_from_user(cmdev->tx.buf, buf, bytes);
		/*
		for (i = 0; i < bytes; i++) {
			writel(mmio_buf[i], cmdev->base + LOGGING_DR);
		}
	        */
		cmdev->tx.sg.length = bytes;
		if (dma_map_sg(cmdev->dev->parent, &cmdev->tx.sg, 1, DMA_TO_DEVICE) != 1) {
			printk(KERN_ERR "Logging Dev: unable to map DMA tx\n");
			break;
		}

		desc = dmaengine_prep_slave_sg(cmdev->tx.chan, &cmdev->tx.sg, 1, \
					       DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
		if (!desc) {
			printk(KERN_ERR "Logging Dev: DMA tx prep error\n");
			dma_unmap_sg(cmdev->dev->parent, &cmdev->tx.sg, 1, DMA_TO_DEVICE);
			break;
		}

		desc->callback = logging_dma_tx_callback;
		desc->callback_param = cmdev;

		cmdev->tx.queued = true;

		cmdev->tx.cookie = dmaengine_submit(desc);
		dma_async_issue_pending(cmdev->tx.chan);
		
		buf += bytes;
		len -= bytes;
		written_bytes += bytes;
	}

	wait_event_interruptible(cmdev->tx_wq, !cmdev->tx.queued);
	*ppos += written_bytes;

	return written_bytes;
}

static long comento_logging_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	struct comento_logging_device *cmdev;
	cmdev = comento_logging_get_device(iminor(fp->f_inode));
	
	switch (cmd) {
	case LOGGING_DEV_IOCTL_CLEAR:
		write_lock(&cmdev->lock);
		writel(LOGGING_RM, cmdev->base + LOGGING_RR);
		write_unlock(&cmdev->lock);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t logging_button_interrupt(int irq, void *dev_id) {
	struct comento_logging_device *cmdev = dev_id;

	writel(LOGGING_RM, cmdev->base + LOGGING_RR);
	
	return IRQ_HANDLED;
}
/*
static ssize_t comento_mmio_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos) {
	struct comento_mmio_device *cmdev;
	ssize_t read_bytes = 0;

	cmdev = comento_mmio_get_device(iminor(fp->f_inode));

	read_lock(&cmdev->lock);
	if (cmdev->rx_size <= len + *ppos) {
		len = cmdev->rx_size - *ppos;
	}
	read_bytes = len - copy_to_user(buf, cmdev->rx_buf + *ppos, len);
	*ppos += read_bytes;
	read_unlock(&cmdev->lock);

	return read_bytes;
}
*/
/*
static irqreturn_t comento_logging_interrupt(int irq, void *dev_id) {
	struct comento_logging_device *cmdev = dev_id;
	unsigned long mis;
	mis = readl(cmdev->base + LOGGING_MIS);

	if (mis & LOGGING_INT_TX) {
		writel(LOGGING_INT_TX, cmdev->base + LOGGING_ICR);  // ris register clear
		//wake_up_interruptible(&cmdev->tx_wq);               // wake up processes form waiting queue

		return IRQ_HANDLED;
	}
	
	if (mis & COMENTO_INT_RX) {
		writel(COMENTO_INT_RX, cmdev->base + COMENTO_ICR);  // ris register clear
		schedule_work(&cmdev->rx_work);                     // reserve bottom-half execution

		return IRQ_HANDLED;
	}
	
	return IRQ_NONE;
}
*/
/*
static void comento_mmio_rx_work_handler(struct work_struct *work) {
	struct comento_mmio_device *cmdev = container_of(work, struct comento_mmio_device, rx_work);
	unsigned long flag, i;

	write_lock(&cmdev->lock);        // about cmdev->rx_buf
	for (i = 0, flag = readl(cmdev->base + COMENTO_FR);
	     !(flag & COMENTO_FLAG_RXFE);
	     flag = readl(cmdev->base + COMENTO_FR), i++) {
		cmdev->rx_buf[i] = readl(cmdev->base + COMENTO_DR);
	}
	cmdev->rx_size = i;
	write_unlock(&cmdev->lock);
}
*/
MODULE_AUTHOR("KuChul<ck08107@gmail.com>");
MODULE_DESCRIPTION("ARM AMBA Comento LOGGING DEV Driver");
MODULE_LICENSE("GPL");
