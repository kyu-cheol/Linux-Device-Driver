#include <linux/kernel.h>
#include <linux/syscalls.h>

#define MAX_QUEUE_SIZE (256 / 4)

static int kernel_queue[MAX_QUEUE_SIZE];
static int front, rear = 0;

int is_empty(void){
	if(front == rear)
		return 1;

	return 0;
}

int is_full(void){
	if((rear + 1) % MAX_QUEUE_SIZE == front)
		return 1;

	return 0;
}

SYSCALL_DEFINE1(enqueue_item, int, item){
	if(is_full()){
		printk(KERN_ERR "Queue is full! cancle enqueue %d\n", item);
		return -1;
	}

	rear = (rear + 1) % MAX_QUEUE_SIZE;
	kernel_queue[rear] = item;

	return 0;
}

SYSCALL_DEFINE0(dequeue_item){
	if(is_empty()){
		printk(KERN_ERR "Queue is empty! Cannot dequeue\n");
		return -1;
	}

	front = (front + 1) % MAX_QUEUE_SIZE;

	return kernel_queue[front];
}
