#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#define DEV_MEM_SIZE 512
#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt,__func__
/* psedo device's memory */
char device_buffer[DEV_MEM_SIZE];

dev_t device_number;
struct cdev pcd_cdev;


loff_t pcd_lseek(struct file *filep, loff_t off , int whence){
	pr_info("lseek requested \n");
	return 0;
}
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *pos){
	pr_info("read requested %zu bytes\n",count);
	return 0;
}

ssize_t pcd_write(struct file *flip, const char __user *buff, size_t count, loff_t *pos){
	pr_info("write requested %zu bytes\n",count);
	return 0;
}

int pcd_open(struct inode *inode, struct file *flip){
	pr_info("Opend successfull\n");
	return 0;
}
int pcd_release (struct inode *inode, struct file *flip){
	pr_info("release successfull \n");
	return 0;
}

struct file_operations pcd_fops = {
	.open = pcd_open,
	.write = pcd_write,
	.read = pcd_read,
	.llseek = pcd_lseek,
	.owner = THIS_MODULE
};

struct class *class_pcd;
struct device *device_pcd;


static int __init pcd_driver_init(void){
	
	/*1. Dynamically allocate a device number*/	
	alloc_chrdev_region(&device_number, 0, 1, "pcd_devices");
	
	pr_info("Device number <major>:<minor> = %d:%d\n",MAJOR(device_number), MINOR(device_number));
	/*2. make the char device registration with the VFS */
	cdev_init(&pcd_cdev, &pcd_fops);	
	
	/*3. register a device with VFS */	
	pcd_cdev.owner = THIS_MODULE;
	cdev_add(&pcd_cdev, device_number, 1);
	
	/*4. lets create a device file */
	class_pcd = class_create("pcd_class");
	
	/*5. populate sysfs with device information */
	device_pcd = device_create(class_pcd, NULL, device_number, NULL, "pcd");
	pr_info("Mdule init successfull \n");
	return 0;	
}

static void __exit pcd_driver_cleanup(void){
	device_destroy(class_pcd, device_number);
	class_destroy(class_pcd);
	cdev_del(&pcd_cdev);
	unregister_chrdev_region(device_number, 1);

	pr_info("Module unloaded\n");
}


module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("DLSHAD");
MODULE_DESCRIPTION("A PSUDO CHAR Driver");
