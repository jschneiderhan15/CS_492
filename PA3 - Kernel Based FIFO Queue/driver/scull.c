/*
 * main.c -- the bare scull char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/mutex.h> 

#include <linux/uaccess.h>	/* copy_*_user */

#include "scull.h"		/* local definitions */
#include "access_ok_version.h"

/*
 * Our parameters which can be set at load time.
 */

static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_fifo_elemsz = SCULL_FIFO_ELEMSZ_DEFAULT; /* SIZE */
static int scull_fifo_size   = SCULL_FIFO_SIZE_DEFAULT; /* N */
char *arr;
char *start, *end;
struct semaphore empty;
struct semaphore full;
static DEFINE_MUTEX(mut_lock);

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_fifo_size, int, S_IRUGO);
module_param(scull_fifo_elemsz, int, S_IRUGO);

MODULE_AUTHOR("Wonderful student of CS-492");
MODULE_LICENSE("Dual BSD/GPL");

static struct cdev scull_cdev;		/* Char device structure		*/


/*
 * Open and close
 */

static int scull_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull open\n");
	return 0;          /* success */
}

static int scull_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull close\n");
	return 0;          /* success */
}

/*
 * Read and Write
 */
static ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	//printk(KERN_INFO "scull read\n");
	int cpy;
	int copiedBytes;
	//locking mutex and semaphore
	if(down_interruptible(&full)){
		return -EINTR;
	}
	if(mutex_lock_interruptible(&mut_lock)){
		return -EINTR;
	}
	//if count is smaller than size of next full element,
	//then only count bytes are copied into buf
	if(count < *(int*) start){
		cpy = copy_to_user(buf, start + sizeof(int) , count);
		copiedBytes = count;
	}
	//copying the bytes of next full element into buf
	else{
		cpy = copy_to_user(buf, start + sizeof(int), *(int*) start);
		copiedBytes = *(int*) start - cpy;
	}
	//if the copy fails, error out
	if(cpy < 0){
		printk(KERN_ERR "ERROR: Copying from array to buffer failed!\n");
		return -EFAULT;
	}
	//pointer arithmetic
	start = start + scull_fifo_elemsz + sizeof(count);
	if(start >= &arr[scull_fifo_size]){
		start = arr;
	}
	//unlocking mutex and semaphore
	mutex_unlock(&mut_lock);
	up(&empty);
	return copiedBytes;
}


static ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	//printk(KERN_ALERT "scull write\n");
	int cpy; 
	int copiedBytes;
	//locking semaphore and mutex
	if(down_interruptible(&empty)){
		return -EINTR;
	}
	if(mutex_lock_interruptible(&mut_lock)){
		return -EINTR;
	}
	//if count is larger than ELEMSZ, only ELEMSZ bytes are copied
	if(count > scull_fifo_elemsz){
		cpy = copy_from_user(end + sizeof(int), buf, scull_fifo_elemsz);
		copiedBytes = scull_fifo_elemsz;
	}
	else{ //else copy count bytes
		cpy = copy_from_user(end + sizeof(int), buf, count);
		copiedBytes = count;
	}
	//if the copy fails, error out
	if(cpy < 0){
		printk(KERN_ERR "ERROR: Copying from buffer to array failed!\n");
		return -EFAULT;
	}
	//pointer arithmetic
	end = end + scull_fifo_elemsz + sizeof(count);
	if(end >= &arr[scull_fifo_size]){
		end = arr;
	}
	//unlocking semaphore and mutex
	mutex_unlock(&mut_lock);
	up(&full);
	return 0;
}

/*
 * The ioctl() implementation
 */

static long scull_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{

	int err = 0;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok_wrapper(VERIFY_WRITE, (void __user *)arg,
				_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok_wrapper(VERIFY_READ, (void __user *)arg,
				_IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
	case SCULL_IOCGETELEMSZ:
		return scull_fifo_elemsz;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;

}


struct file_operations scull_fops = {
	.owner 		= THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open 		= scull_open,
	.release	= scull_release,
	.read 		= scull_read,
	.write 		= scull_write,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);

	/* Free FIFO safely */
	kfree(arr);	
	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
}


int scull_init_module(void)
{
	int result;
	dev_t dev = 0;
	
	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, 1, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, 1, "scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	cdev_init(&scull_cdev, &scull_fops);
	scull_cdev.owner = THIS_MODULE;
	result = cdev_add (&scull_cdev, dev, 1);
	/* Fail gracefully if need be */
	if (result) {
		printk(KERN_NOTICE "Error %d adding scull character device", result);
		goto fail;
	}
	//semaphores
	sema_init(&full, 0);
	sema_init(&empty, scull_fifo_size);
	
	/* Allocate FIFO correctly */
	arr = kmalloc(scull_fifo_elemsz + sizeof(scull_fifo_elemsz), GFP_KERNEL);

	if(!arr){
		printk(KERN_ERR "ERROR: FIFO unable to be allocated!\n");
		return -EFAULT;
	}
	start = arr;
	end = arr;
	//printk(KERN_INFO "scull: FIFO SIZE=%u, ELEMSZ=%u\n", scull_fifo_size, scull_fifo_elemsz);

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
