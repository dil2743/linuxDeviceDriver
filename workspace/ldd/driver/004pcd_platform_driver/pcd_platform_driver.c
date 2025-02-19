#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include "platform.h"
#include <linux/slab.h>
#include <linux/mod_devicetable.h>

#define MAX_DEVICES 10

#undef pr_fmt

#define pr_fmt(fmt) "%s:" fmt, __func__

/* device private data structure */
struct pcdev_private_data {
	struct pcdev_platform_data *pdata;
	char *buffer;
	dev_t dev_num;
	struct cdev cdev;
};

/* driver private data structure */
struct pcdrv_private_data {
	int total_devices;
	dev_t device_num_base;
	struct class *class_pcd;
	struct device *device_pcd;
};

struct pcdrv_private_data pcdrv_data;

int pcd_release(struct inode *inode, struct file *filp);
loff_t pcd_lseek(struct file *filp, loff_t offset, int whence);
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filp);
int check_permission(int dev_perm, int acc_mode);

loff_t pcd_lseek(struct file *filp, loff_t offset, int whence) {
	struct pcdev_private_data *dev_data = (struct pcdev_private_data *)filp->private_data;
	loff_t temp;

	pr_info("lseek requested \n");

	switch (whence) {
		case SEEK_SET:
			if (offset > dev_data->pdata->size || offset < 0)
				return -EINVAL;
			filp->f_pos = offset;
			break;
		case SEEK_CUR:
			temp = filp->f_pos + offset;
			if (temp > dev_data->pdata->size || temp < 0)
				return -EINVAL;
			filp->f_pos = temp;
			break;
		case SEEK_END:
			temp = dev_data->pdata->size + offset;
			if (temp > dev_data->pdata->size || temp < 0)
				return -EINVAL;
			filp->f_pos = temp;
			break;
		default:
			return -EINVAL;
	}

	return filp->f_pos;
}

ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos) {
	struct pcdev_private_data *dev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = dev_data->pdata->size;

	pr_info("read requested %zu bytes\n", count);

	if ((*f_pos + count) > max_size)
		count = max_size - *f_pos;

	if (copy_to_user(buff, dev_data->buffer + *f_pos, count))
		return -EFAULT;

	*f_pos += count;

	pr_info("Number of bytes successfully read = %zu\n", count);

	return count;
}

ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos) {
	struct pcdev_private_data *dev_data = (struct pcdev_private_data *)filp->private_data;
	int max_size = dev_data->pdata->size;

	pr_info("write requested %zu bytes\n", count);

	if ((*f_pos + count) > max_size)
		count = max_size - *f_pos;

	if (!count)
		return -ENOMEM;

	if (copy_from_user(dev_data->buffer + *f_pos, buff, count))
		return -EFAULT;

	*f_pos += count;

	pr_info("Number of bytes successfully written = %zu\n", count);

	return count;
}

int check_permission(int dev_perm, int acc_mode) {
	if (dev_perm == RDWR)
		return 0;
	if ((dev_perm == RDONLY) && ((acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ)))
		return -EPERM;
	if ((dev_perm == WRONLY) && ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE)))
		return -EPERM;

	return 0;
}

int pcd_open(struct inode *inode, struct file *filp) {
	int ret;
	int minor_n;
	struct pcdev_private_data *dev_data;

	minor_n = MINOR(inode->i_rdev);
	pr_info("open requested for device %d\n", minor_n);

	dev_data = container_of(inode->i_cdev, struct pcdev_private_data, cdev);
	filp->private_data = dev_data;

	ret = check_permission(dev_data->pdata->perm, filp->f_mode);

	(!ret) ? pr_info("open was successful\n") : pr_info("open was unsuccessful\n");

	return ret;
}

int pcd_release(struct inode *inode, struct file *filp) {
	pr_info("release requested\n");
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
static void __exit pcd_platform_device_cleanup(void);

struct platform_device_id pcdev_ids[] = {
	[0] = { .name = "pseudo-char-device-A1X", .driver_data = 0},
	[1] = { .name = "pseudo-char-device-B1X", .driver_data = 1},
	[2] = { .name = "pseudo-char-device-C1X", .driver_data = 2},
	[3] = { .name = "pseudo-char-device-D1X", .driver_data = 3},
	{ } /* Null terminator */
};
MODULE_DEVICE_TABLE(platform, pcdev_ids);

struct platform_driver pcd_platform_driver = {
	.probe = pcd_platform_device_probe,
	.remove = pcd_platform_device_remove,
	.id_table = pcdev_ids,
	.driver = {
		.name = "pseudo-char-device",  // Base name for matching
		.owner = THIS_MODULE
	}
};

int pcd_platform_device_remove(struct platform_device *pdev) {
	pr_info("A device is removed\n");

	/* get device private data */
	struct pcdev_private_data *dev_data = (struct pcdev_private_data *)platform_get_drvdata(pdev);
	/* remove device file */
	device_destroy(pcdrv_data.class_pcd, dev_data->dev_num);
	/* remove cdev entry */
	cdev_del(&dev_data->cdev);

	pcdrv_data.total_devices--;

	return 0;
}

int pcd_platform_device_probe(struct platform_device *pdev) {
	pr_info("A device is detected\n");

	const struct platform_device_id *id;
	int ret;

	/* First check if we have a valid device */
	if (!pdev->name) {
		pr_err("Device name is missing\n");
		return -EINVAL;
	}

	id = platform_get_device_id(pdev);
	if (!id) {
		pr_err("No matching platform device id found\n");
		return -EINVAL;
	}

	pr_info("Device name = %s\n", pdev->name);
	pr_info("Device id = %d\n", pdev->id);
	pr_info("Driver data = %ld\n", id->driver_data);

	/* get platform data */
	struct pcdev_platform_data *pdata = (struct pcdev_platform_data *)dev_get_platdata(&pdev->dev);
	if (!pdata) {
		pr_info("No platform data available\n");
		return -EINVAL;
	}

	pr_info("Platform data: size=%d, perm=%d, serial_number=%s\n", pdata->size, pdata->perm, pdata->serial_number);

	/* Dynamically allocate memory for the device private data */
	struct pcdev_private_data *dev_data = devm_kzalloc(&pdev->dev, sizeof(struct pcdev_private_data), GFP_KERNEL);
	if (!dev_data) {
		pr_err("Cannot allocate memory\n");
		return -ENOMEM;
	}

	/* allocate memory for pdata */
	dev_data->pdata = devm_kzalloc(&pdev->dev, sizeof(struct pcdev_platform_data), GFP_KERNEL);
	if (!dev_data->pdata) {
		pr_err("Cannot allocate memory for pdata\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, dev_data);

	/* copy platform data to device private data */
	memcpy(dev_data->pdata, pdata, sizeof(struct pcdev_platform_data));

	/* save device private data pointer in platform device structure */
	dev_data->pdata->size = pdata->size;
	dev_data->pdata->perm = pdata->perm;
	dev_data->pdata->serial_number = pdata->serial_number;

	pr_info("Device serial number = %s\n", dev_data->pdata->serial_number);
	pr_info("Device size = %d\n", dev_data->pdata->size);
	pr_info("Device permission = %d\n", dev_data->pdata->perm);

	dev_data->buffer = devm_kzalloc(&pdev->dev, dev_data->pdata->size, GFP_KERNEL);
	if (!dev_data->buffer) {
		pr_err("Cannot allocate memory\n");
		return -ENOMEM;
	}

	/* get device number */
	dev_data->dev_num = pcdrv_data.device_num_base + pdev->id;

	/* cdev init */
	cdev_init(&dev_data->cdev, &pcd_fops);
	dev_data->cdev.owner = THIS_MODULE;

	/* add device to the system */
	ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
	if (ret < 0) {
		pr_err("cdev add failed\n");
		return ret;
	}
	/* create device file for detected device */
	pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, dev_data->dev_num, NULL, "pcdev-%d", pdev->id);
	if (IS_ERR(pcdrv_data.device_pcd)) {
		pr_err("Device creation failed\n");
		cdev_del(&dev_data->cdev);
		return PTR_ERR(pcdrv_data.device_pcd);
	}

	pcdrv_data.total_devices++;

	pr_info("Probe was successful\n");

	return 0;
}

static int __init pcd_driver_init(void) {
	int ret;

	pr_info("Initializing platform driver\n");
	
	/* dynamically allocate a device number for MAX devices */
	ret = alloc_chrdev_region(&pcdrv_data.device_num_base, 0, MAX_DEVICES, "pcd_devices");
	if (ret < 0) {
		pr_err("Alloc chrdev failed\n");
		return ret;
	}
	
	pr_info("Device number <major>:<minor> = %d:%d\n", MAJOR(pcdrv_data.device_num_base), MINOR(pcdrv_data.device_num_base));
	
	/* create device class under /sys/class */
	pcdrv_data.class_pcd = class_create("pcd_class");
	if (IS_ERR(pcdrv_data.class_pcd)) {
		pr_err("Class creation failed\n");
		ret = PTR_ERR(pcdrv_data.class_pcd);
		unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);
		return ret;
	}

	pr_info("Class created successfully\n");

	/* register platform driver */
	ret = platform_driver_register(&pcd_platform_driver);
	if (ret < 0) {
		pr_err("Platform driver registration failed\n");
		class_destroy(pcdrv_data.class_pcd);
		unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);
		return ret;
	}

	pr_info("Platform driver registered successfully with name: %s\n", pcd_platform_driver.driver.name);
	return 0;
}

static void __exit pcd_driver_cleanup(void) {
	/* unregister platform driver */
	platform_driver_unregister(&pcd_platform_driver);
	pr_info("pcd platform driver unloaded\n");
	/* destroy devices */
	class_destroy(pcdrv_data.class_pcd);
	/* unregister device number range */
	unregister_chrdev_region(pcdrv_data.device_num_base, MAX_DEVICES);

	pr_info("Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DLSHAD");
MODULE_DESCRIPTION("A PSEUDO CHAR Driver");
