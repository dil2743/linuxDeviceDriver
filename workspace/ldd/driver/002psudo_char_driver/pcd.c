#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>

#define DEV_MEM_SIZE 512
#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt,__func__
/* psedo device's memory */
char device_buffer[DEV_MEM_SIZE];

dev_t device_number;
struct cdev pcd_cdev;


loff_t pcd_lseek(struct file *filp, loff_t offset , int whence){
	pr_info("lseek requested \n");
	loff_t tmp;

	
	pr_info("current file pointer is at %lld\n",filp->f_pos);
	switch(whence){
	
		case SEEK_SET:
			if((offset > DEV_MEM_SIZE) || (offset < 0))
				return -EINVAL;
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			tmp = (filp->f_pos + offset);
			if( tmp > DEV_MEM_SIZE ||  tmp < 0 )
				return -EINVAL;
			filp->f_pos += offset;
			break;
		case SEEK_END:
			tmp = DEV_MEM_SIZE + offset;
			if(tmp > DEV_MEM_SIZE || tmp <0 )
				return -EINVAL;
			filp->f_pos = DEV_MEM_SIZE + offset; 
			break;
		default:
			return -EINVAL;

	}
	pr_info("Now value of the file position is %lld\n",filp->f_pos);
	return filp->f_pos;
}
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos){
	pr_info("read requested %zu bytes\n",count);
	pr_info("Current file position %lld\n", *f_pos);
	/*1. check user requested count value as the device has limited storage */
	if((*f_pos + count) > DEV_MEM_SIZE)
		count = DEV_MEM_SIZE - *f_pos;
	/*2. copy count number of bytes from device memory to user buffer*/
	if((copy_to_user(buff, &device_buffer[*f_pos], count)))
		return -EFAULT;
	/*3. update the f_pos position pointer */
	*f_pos += count;
	
	pr_info("Number of bytes succeffult read = %zu, \n", count);
	pr_info("File postion udated = %lld\n",*f_pos);
	/*4. return number of bytes succeffuly read or error code */
	/*5. if f_ops at EOF, then rerurn 0 */
	return count;
}

ssize_t pcd_write(struct file *flip, const char __user *buff, size_t count, loff_t *f_pos){
	
	pr_info("write requested %zu bytes\n",count);
        pr_info("Current file position %lld\n", *f_pos);

        /*1. check user requested count value as the device has limited storage */
        if((*f_pos + count) > DEV_MEM_SIZE)
                count = DEV_MEM_SIZE - *f_pos;

	if(!count){
		pr_err("No Memory Left on device!!!\n");
		return -ENOMEM;
	}
        /*2. copy count number of bytes from device memory to user buffer*/
        if(copy_from_user(&device_buffer[*f_pos], buff, count))
                return -EFAULT;
        /*3. update the f_pos position pointer */
        *f_pos += count;

        pr_info("Number of bytes succeffult written = %zu, \n", count);
        pr_info("File postion udated = %lld\n",*f_pos);
        /*4. return number of bytes successfully written or error code */
        /*5. if f_ops at EOF, then rerurn 0 */
        return count;
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
	.release = pcd_release,
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
