
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
KER_DIR=/home/hkd/deviceDriverDevelopment/workspace/ldd/source/linux-6.1.83-ti-rt-r37/
HOST_KER_DIR= /lib/modules/$(shell uname -r)/build/
obj-m := pcd_device_setup.o pcd_platform_driver.o

all:
	sudo make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KER_DIR)  M=$(PWD) modules

clean:
	sudo make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KER_DIR)  M=$(PWD) clean

help:
	sudo make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KER_DIR)  M=$(PWD) help

host:
	sudo make -C $(HOST_KER_DIR)  M=$(PWD) modules
