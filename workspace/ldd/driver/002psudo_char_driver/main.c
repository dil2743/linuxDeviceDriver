#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>

#define DEV_MEM_SIZE 512

* psedo device's memory */
char device_buffer[DEV_MEM_SIZE];

dev_t device_number;
struct cdev pcd_cdev;
struct file_operations pcd_fops;

static int __init pcd_driver_init(void){
	
	/*1. Dynamically allocate a device number*/	
	alloc_chrdev_region(&device_number, 0, 1, "pcd");
	/*2. make the char device registration with the VFS */
	cdev_init(&pcd_cdev, &pcd_fops);	
	pcd_cdev.owner = THIS_MODULE;
	/* register a device with VFS */
	cdev_add(&pcd_dev, device_number, 1);
	
	/*3. lets create a device file */
	
	return 0;	
}

static void __exit pcd_driver_cleanup(void){

}


module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("DLSHAD");
MODULE_DESCRIPTION("A PSUDO CHAR Driver");
