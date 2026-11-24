#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/spinlock.h>

#define DEVICE_NAME "ring_buffer"
#define CLASS_NAME "example"
#define RINGBUF_SIZE 4096

static int ringbuf_device_major;
static struct class *example_class;
static struct device *ring_buffer_device;

static DEFINE_RWLOCK(ringbuf_spin_lock);
static char ring_buf[RINGBUF_SIZE] = {0,};

static int ringbuf_front = 0;
static int ringbuf_rear = 0;

// ======================================= QUEUE ADT ==========================================

static int ringbuf_full(char *ring_buf){
	read_lock(&ringbuf_spin_lock);
	if((ringbuf_rear + 1) % RINGBUF_SIZE == ringbuf_front){
		read_unlock(&ringbuf_spin_lock);
		return 1;
	}
	else{
		read_unlock(&ringbuf_spin_lock);
		return 0;
	}
}

static int ringbuf_empty(char *ring_buf){
	read_lock(&ringbuf_spin_lock);
	if(ringbuf_front == ringbuf_rear){
	      	read_unlock(&ringbuf_spin_lock);
		return 1;
	}
	else{
		read_unlock(&ringbuf_spin_lock);
		return 0;
	}
}

static void enqueue_ringbuf(char *ring_buf, char item){
	write_lock(&ringbuf_spin_lock);  // write lock
	ringbuf_rear = (ringbuf_rear + 1) % RINGBUF_SIZE;
	ring_buf[ringbuf_rear] = item;
	write_unlock(&ringbuf_spin_lock); // write unlock
}

static char dequeue_ringbuf(char *ring_buf){
	char ret;
	//nested lock
	//write_lock(&ringbuf_spin_lock);  // write lock
	ringbuf_front = (ringbuf_front + 1) % RINGBUF_SIZE;
	//write_unlock(&ringbuf_spin_lock);  // write unlock
	//read_lock(&ringbuf_spin_lock);   // read lock
	ret = ring_buf[ringbuf_front];
	//read_unlock(&ringbuf_spin_lock); // read unlock
	
	return ret;
}

// ===============================================================================================

static int ring_buf_open(struct inode *inode, struct file *fp){
	printk(KERN_DEBUG "%s : open %s\n", __func__, DEVICE_NAME);

	return 0;
}

static ssize_t ring_buf_write(struct file *fp, const char __user *buf, size_t len, loff_t *ppos){
	int i, read_bytes = 0;
	char item;
	printk(KERN_DEBUG "%s\n", __func__);

	if(ringbuf_full(ring_buf)){
		printk(KERN_DEBUG "Fail to write. already full.\n");
		return -1;
	}
	
	for(i = 0; i < (int)len; i++){
		write_lock(&ringbuf_spin_lock);   // write lock
		copy_from_user(&item, &buf[i], 1);
		write_unlock(&ringbuf_spin_lock); // write unlock
		enqueue_ringbuf(ring_buf, item);
		read_bytes++;
		if(ringbuf_full(ring_buf))
			break;
	}

	return read_bytes;
}

static ssize_t ring_buf_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos){
	int i, written_bytes = 0;
	char item;
	printk(KERN_DEBUG "%s\n", __func__);

	if(ringbuf_empty(ring_buf)){
		printk(KERN_DEBUG "Fail to read. already empty.\n");
		return -1;
	}
	
	for(i = 0; i < len; i++){
		write_lock(&ringbuf_spin_lock);  // write lock
		item = dequeue_ringbuf(ring_buf);
		write_unlock(&ringbuf_spin_lock);  // write unlock
		
		read_lock(&ringbuf_spin_lock);  // read lock
		copy_to_user(&buf[i], &item, 1);
		read_unlock(&ringbuf_spin_lock); // read unlock
		written_bytes++;
		if(ringbuf_empty(ring_buf))
			break;
	}

	return written_bytes;
}

/*
static loff_t ring_buf_lseek(struct file *fp, const loff_t offset, int whence){
	printk(KERN_DEBUG "%s\n", __func__);
	fp->f_pos = offset;

	return offset;
}
*/

static int ring_buf_close(struct inode *inode, struct file *fp){
	printk(KERN_DEBUG "%s\n", __func__);

	return 0;
}

static struct file_operations ringbuf_device_fops = {
	.open = ring_buf_open,
	.read = ring_buf_read,
	.write = ring_buf_write,
	.release = ring_buf_close,
	//.llseek = ring_buf_lseek,
};

static int __init ringbuf_module_init(void){
	int minor = 0;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);
	
	ringbuf_device_major = register_chrdev(0, DEVICE_NAME, &ringbuf_device_fops);
	if(ringbuf_device_major < 0){
		printk(KERN_DEBUG "%s : Failed register_chrdev.\n", DEVICE_NAME);
		ret = ringbuf_device_major;
		goto err_register_chrdev;
	}

	example_class = class_create(CLASS_NAME);
	if(IS_ERR(example_class)){
		printk(KERN_DEBUG "%s : Failed class_create.\n", DEVICE_NAME);
		ret = PTR_ERR(example_class);
		goto err_class_create;
	}

	ring_buffer_device = device_create(example_class, NULL, MKDEV(ringbuf_device_major, minor), NULL, "%s%d", \
				       DEVICE_NAME, minor);
	if(IS_ERR(&ring_buffer_device[minor])){
		printk(KERN_DEBUG "%s : Failed device_create.\n", DEVICE_NAME);
		ret = PTR_ERR(&ring_buffer_device[minor]);
		goto err_device_create;
	}

	return 0;

 err_device_create:
	class_destroy(example_class);
 err_class_create:
	unregister_chrdev(ringbuf_device_major, DEVICE_NAME);
 err_register_chrdev:
	return ret;
}

static void __exit ringbuf_module_exit(void){
	printk(KERN_DEBUG "%s\n", __func__);
	class_destroy(example_class);
	unregister_chrdev(ringbuf_device_major, DEVICE_NAME);
}

module_init(ringbuf_module_init);
module_exit(ringbuf_module_exit);

MODULE_AUTHOR("kuchul<ck08107@gmail.com>");
MODULE_DESCRIPTION("An example driver");
MODULE_LICENSE("GPL v2");

