# 10_device_driver/Makefile
# kbuild Makefile for the character device driver.
# Usage:
#   make KDIR=/lib/modules/$(uname -r)/build
#   sudo insmod chardev.ko
#   ls -la /dev/chardev0
#   echo "test data" > /dev/chardev0
#   cat /dev/chardev0
#   sudo rmmod chardev
#   dmesg | tail -20

obj-m += chardev.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Helper: quick load/unload cycle
reload: clean all
	-sudo rmmod chardev
	sudo insmod chardev.ko
	ls -la /dev/chardev0
	dmesg | tail -10

.PHONY: all clean reload
