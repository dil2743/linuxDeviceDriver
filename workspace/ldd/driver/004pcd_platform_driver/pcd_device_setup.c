#include <linux/module.h>
#include <linux/platform_device.h>
#include "platform.h"

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt,__func__

/* create four platform data */
struct pcdev_platform_data pcdev_pdata[4]= {
	[0] = { .size = 512, .perm = RDWR, .serial_number ="PCDEVABC1111"},
	[1] = { .size = 1024, .perm = RDWR, .serial_number ="PCDEVXYZ2222"},
	[2] = { .size = 2048, .perm = RDWR, .serial_number ="PCDEVLMN3333"},
	[3] = { .size = 4096, .perm = RDWR, .serial_number ="PCDEVOPQ4444"},
};

static int __init pcdev_platform_init(void);
static void __exit pcdev_platform_exit(void);
void pcdev_release(struct device *dev);

void pcdev_release(struct device *dev) {
	pr_info("Device is released!");
}

/* create four platform devices */

struct platform_device platform_pcdev_1 = {
	.name = "pseudo-char-device-A1X",
	.id = 0,
	.dev = {
		.platform_data = &pcdev_pdata[0],
		.release = pcdev_release,
	},
};

struct platform_device platform_pcdev_2 = {
	.name = "pseudo-char-device-B1X",
	.id = 1,
	.dev = {
		.platform_data = &pcdev_pdata[1],
		.release = pcdev_release,
	},
};

struct platform_device platform_pcdev_3 = {
	.name = "pseudo-char-device-C1X",
	.id = 2,
	.dev = {
		.platform_data = &pcdev_pdata[2],
		.release = pcdev_release,
	},
};

struct platform_device platform_pcdev_4 = {
	.name = "pseudo-char-device-D1X",
	.id = 3,
	.dev = {
		.platform_data = &pcdev_pdata[3],
		.release = pcdev_release,
	},
};

struct platform_device *pcd_platform_devs[] = {
	&platform_pcdev_1,
	&platform_pcdev_2,
	&platform_pcdev_3,
	&platform_pcdev_4,
};


static int __init pcdev_platform_init(void) {
	int ret;

	pr_info("Registering platform devices\n");

	ret = platform_add_devices(pcd_platform_devs, ARRAY_SIZE(pcd_platform_devs));
	if (ret) {
		pr_err("Failed to add platform devices, error=%d\n", ret);
		return ret;
	}

	pr_info("All platform devices registered successfully\n");
	pr_info("Device 1: name=%s, id=%d\n", platform_pcdev_1.name, platform_pcdev_1.id);
	pr_info("Device 2: name=%s, id=%d\n", platform_pcdev_2.name, platform_pcdev_2.id);
	pr_info("Device 3: name=%s, id=%d\n", platform_pcdev_3.name, platform_pcdev_3.id);
	pr_info("Device 4: name=%s, id=%d\n", platform_pcdev_4.name, platform_pcdev_4.id);

	return 0;
}

static void __exit pcdev_platform_exit(void) {
	platform_device_unregister(&platform_pcdev_1);
	platform_device_unregister(&platform_pcdev_2);
	platform_device_unregister(&platform_pcdev_3);
	platform_device_unregister(&platform_pcdev_4);

	pr_info("Device setup module unloaded\n");
}

module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module which registers platform devices");
