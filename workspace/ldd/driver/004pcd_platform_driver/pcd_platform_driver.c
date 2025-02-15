#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>
#include<linux/platform_device.h>


#define NO_OF_DEVICES 4
#define MEM_SIZE_MAX_DEV_PCDEV1 1024
#define MEM_SIZE_MAX_DEV_PCDEV2 512
#define MEM_SIZE_MAX_DEV_PCDEV3 1024
#define MEM_SIZE_MAX_DEV_PCDEV4 512

#define RDONLY 0x01
#define WRONLY 0x10
#define RDWR 0x11

#undef pr_fmt

#define pr_fmt(fmt) "%s:" fmt,__func__

/* pseudo device's memory */
char device_buffer_pcdev1[MEM_SIZE_MAX_DEV_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_DEV_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_DEV_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_DEV_PCDEV4];

/* device private data structure */
struct pcdev_private_data{
	char *buffer;
	unsigned size;
	const char *serial_number;
	int perm; //permission
	struct cdev cdev;
};

/* driver private data structure */
struct pcdrv_private_data{
	int total_devices;
	dev_t device_number;
	struct class *class_pcd;
	struct device *device_pcd;
	struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};

struct pcdrv_private_data pcdrv_data = {
	.total_devices = NO_OF_DEVICES,
	.pcdev_data = {
		[0] = {
			.buffer = device_buffer_pcdev1,
			.size = MEM_SIZE_MAX_DEV_PCDEV1,
			.serial_number = "PCDDEVE1XYZ123",
			.perm = 0x1, /* RD */
		},
		[1] = {
			.buffer = device_buffer_pcdev2,
			.size = MEM_SIZE_MAX_DEV_PCDEV2,
			.serial_number = "PCDDEVE2XYZ123",
			.perm = 0x10, /* WR */
		},
		[2] = {
			.buffer = device_buffer_pcdev3,
			.size = MEM_SIZE_MAX_DEV_PCDEV3,
			.serial_number = "PCDDEVE3XYZ123",
			.perm = 0x11, /* RDWR */
		},
		[3] = {
			.buffer = device_buffer_pcdev4,
			.size = MEM_SIZE_MAX_DEV_PCDEV4,
			.serial_number = "PCDDEVE4XYZ123",
			.perm = 0x11, /* RDWR */
		}
	}
};


int pcd_release (struct inode *inode, struct file *filp);
loff_t pcd_lseek(struct file *filp, loff_t offset , int whence);
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filp);
int check_permission( int dev_perm, int acc_mode);


loff_t pcd_lseek(struct file *filp, loff_t offset , int whence){
	pr_info("lseek requested \n");
	loff_t tmp;
	struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = pcdev_data->size;

	pr_info("current file pointer is at %lld\n",filp->f_pos);
	switch(whence){

		case SEEK_SET:
			if((offset > max_size) || (offset < 0))
				return -EINVAL;
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			tmp = (filp->f_pos + offset);
			if( tmp > max_size ||  tmp < 0 )
				return -EINVAL;
			filp->f_pos += offset;
			break;
		case SEEK_END:
			tmp = max_size + offset;
			if(tmp > max_size || tmp <0 )
				return -EINVAL;
			filp->f_pos = max_size + offset;
			break;
		default:
			return -EINVAL;

	}
	pr_info("Now value of the file position is %lld\n",filp->f_pos);
	return filp->f_pos;
}
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos){

	struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = pcdev_data->size;

	pr_info("read requested %zu bytes\n",count);
	pr_info("Current file position %lld\n", *f_pos);
	/*1. check user requested count value as the device has limited storage */
	if((*f_pos + count) > max_size)
		count = max_size - *f_pos;
	/*2. copy count number of bytes from device memory to user buffer*/
	if(copy_to_user(buff, pcdev_data->buffer + (*f_pos), count))
		return -EFAULT;
	/*3. update the f_pos position pointer */
	*f_pos += count;

	pr_info("Number of bytes successfully read = %zu, \n", count);
	pr_info("File position updated = %lld\n",*f_pos);
	/*4. return number of bytes successfully read or error code */
	/*5. if f_ops at EOF, then return 0 */
	return count;
}
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos){
	struct pcdev_private_data *pcdev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = pcdev_data->size;

	pr_info("write requested %zu bytes\n",count);
        pr_info("Current file position %lld\n", *f_pos);

        /*1. check user requested count value as the device has limited storage */
        if((*f_pos + count) > max_size)
                count = max_size - *f_pos;

	if(!count){
		pr_err("No Memory Left on device!!!\n");
		return -ENOMEM;
	}
        /*2. copy count number of bytes from device memory to user buffer*/
        if(copy_from_user(pcdev_data->buffer + (*f_pos), buff, count))
                return -EFAULT;
        /*3. update the f_pos position pointer */
        *f_pos += count;

        pr_info("Number of bytes successfully written = %zu, \n", count);
        pr_info("File position updated = %lld\n",*f_pos);
        /*4. return number of bytes successfully written or error code */
        /*5. if f_ops at EOF, then return 0 */
        return count;
}


int check_permission(int dev_perm, int acc_mode) {
	if (dev_perm == RDWR)
		return 0;

	if ((dev_perm == RDONLY) && ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE)))
		return 0;

	if ((dev_perm == WRONLY) && ((acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ)))
		return 0;

	return -EPERM;
}

int pcd_open(struct inode *inode, struct file *filp){
	int ret;
	struct pcdev_private_data *pcdev_data;

	/* find out which device tries to open the file*/
	int minor_n = MINOR(inode->i_rdev);
	pr_info("minor access = %d\n",minor_n);

	/* get the device's private data structure */
	pcdev_data = container_of(inode->i_cdev, struct pcdev_private_data, cdev);
	/* to supply the private data to other methods of the driver  */
	filp->private_data = pcdev_data;

	/* check permission */
	ret = check_permission(pcdev_data->perm, filp->f_mode);

	if(ret == 0 ){
		pr_info("Opened successfully\n");
	}
	else{
		pr_info("open failed\n");
	}
	return ret;
}
int pcd_release (struct inode *inode, struct file *filp){
	pr_info("release successful \n");
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
int pcd_platform_device_probe(struct platform_device *pdev);
int pcd_platform_device_remove(struct platform_device *pdev);
static int __init pcd_platform_device_init(void);
static void __exit pcd_platfrom_device_cleanup(void);

struct platform_driver pcd_platform_driver = {
	.probe = pcd_platform_device_probe,
	.remove = pcd_platform_device_remove,
	.driver = {
		.name = "pseudo-char-device"
	}
};

int pcd_platform_device_remove(struct platform_device *pdev){
	pr_info("A device is removed\n");
	return 0;
}

int pcd_platform_device_probe(struct platform_device *pdev){

	pr_info("A device is detected\n");
	return 0;
}

static int __init pcd_platform_device_init(void){
	platform_driver_register(&pcd_platform_driver);
	pr_info("PCD platform loaded \n");
	return 0;
}

static void __exit pcd_platform_device_cleanup(void) {
	platform_driver_unregister(&pcd_platform_driver);
	pr_info("pcd platform Cleanup completed");
}




static int __init pcd_driver_init(void){


	return 0;
}

static void __exit pcd_driver_cleanup(void){
	int i;

	for(i=0; i<NO_OF_DEVICES; i++){
		device_destroy(pcdrv_data.class_pcd, pcdrv_data.device_number+i);
		cdev_del(&pcdrv_data.pcdev_data[i].cdev);
	}

	class_destroy(pcdrv_data.class_pcd);
	unregister_chrdev_region(pcdrv_data.device_number, NO_OF_DEVICES);

	pr_info("Module unloaded\n");
}


module_init(pcd_platform_device_init);
module_exit(pcd_platform_device_cleanup);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DLSHAD");
MODULE_DESCRIPTION("A PSEUDO CHAR Driver");
