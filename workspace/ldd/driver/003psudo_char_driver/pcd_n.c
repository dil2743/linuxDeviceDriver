#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>

#define NO_OF_DEVICES 4
#define MEM_SIZE_MAX_DEV_PCDEV1 1024
#define MEM_SIZE_MAX_DEV_PCDEV2 512
#define MEM_SIZE_MAX_DEV_PCDEV3 1024
#define MEM_SIZE_MAX_DEV_PCDEV4 512
#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt,__func__

/* psedo device's memory */
char device_buffer_pcdev1[MEM_SIZE_MAX_DEV_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_DEV_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_DEV_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_DEV_PCDEV4];

/* device private data strucutre */
struct pcdev_private_data{
	char *buffer;
	unsigned size;
	const char *serial_number;
	int perm; //permission
	struct cdev cdev;
};

/* drvier prive data structure */
struct pcdrv_private_data{
	int total_devices;
	dev_t device_number;
	struct class *class_pcd;
	struct device *device_pcd;
	struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};	

struct pcdrv_private_data pcdrv_data = {
	.total_devices = NO_OF_DEVICES;
	.pcdev_data = { 
		[0] = {
			.buffer = device_buffer_pcdev1;
			.size = MEM_SIZE_MAX_PCDEV1;
			.serial_number = PCDDEVE1XYZ123;	
			.perm = 0x1; /* RD */
		},
		[1] = {
			.buffer = device_buffer_pcdev2;
			.size = MEM_SIZE_MAX_PCDEV2;
			.serial_number = PCDDEVE2XYZ123;	
			.perm = 0x10; /* WR */
		},
		[2] = {
			.buffer = device_buffer_pcdev1;
			.size = MEM_SIZE_MAX_PCDEV3;
			.serial_number = PCDDEVE3XYZ123;	
			.perm = 0x11; /* RDWR */
		},
		[3] = {
			.buffer = device_buffer_pcdev1;
			.size = MEM_SIZE_MAX_PCDEV4;
			.serial_number = PCDDEVE4XYZ123;	
			.perm = 0x11; /* RDWR */
		},
	}
};


int pcd_release (struct inode *inode, struct file *flip);
loff_t pcd_lseek(struct file *filp, loff_t offset , int whence);
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *flip, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *flip);


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



static int __init pcd_driver_init(void){
	
	int ret;
	int i;
	/*1. Dynamically allocate a device number*/	
	ret = alloc_chrdev_region(&pcdrv_data.device_number, 0, NO_OF_DEVICES, "pcd_devices");
	if(ret<0)
		goto out;
	
	/*lets create a device file */
	pcdev_data.class_pcd = class_create("pcd_class");
	if(IS_ERR(pcdrv_data.class_pcd)){
		pr_err("Class creation failed\n");
		ret = PTR_ERR(pcdev_data.class_pcd);
		goto unreg_chrdev;
	}

	
	for(i = 0 ; i < NO_OF_DEVICES; i++){
		pr_info("Device number <major>:<minor> = %d:%d\n",MAJOR(pcdrv_data.device_number+i), MINOR(pcdrv_data.device_number+i));

		/*make the char device registration with the VFS */

		cdev_init(&pcdrv_data.pcdev_data[i].cdev, &pcd_fops);	
	
		/*register a device with VFS */	
		pcdrv_data.pcd_data[i].owner = THIS_MODULE;
		ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev, pcdrv_data.device_number+i, 1);
		if(ret<0)
			goto cdev_del;
		/*5. populate sysfs with device information */
		pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, drv_data.device_number+i, NULL, "pcdev-%d",i+1);
		if(IS_ERR(pcdrv_data.device_pcd)){
			pr_err("Device creation failed\n");
			ret = PTR_ERR(pcdrv_data.device_pcd);
			goto class_del;
		
		}
	}
	pr_info("Mdule init successfull \n");
	return 0;	
cdev_del:
class_del:
	for(; i>=0; i--){
		device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number+i);
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);
	}
	class_destroy(pcdrv_data.class_pcd);

unreg_chrdev:
	unregister_chrdev_region(pcdev_data.device_number,NO_OF_DEVICES);
out:
	pr_err("device insertion failed\n");
	return ret;
}

static void __exit pcd_driver_cleanup(void){
	int i;

	for(i=0; i<NO_OF_DEVICES; i++){

		device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number+i);
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);
		
	}
	
	class_destroy(pcdrv_data.class_pcd);
	unregister_chrdev_region(pcdrv_data.device_number. NO_OF_DEVICES);

	pr_info("Module unloaded\n");
}


module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("DLSHAD");
MODULE_DESCRIPTION("A PSUDO CHAR Driver");
