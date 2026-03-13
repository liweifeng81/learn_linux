# 09_kernel_module/Makefile
# kbuild Makefile — used to build out-of-tree kernel modules.
# Usage:
#   make KDIR=/lib/modules/$(uname -r)/build
#   sudo insmod hello_module.ko name="Test" count=5
#   cat /proc/hello_module
#   sudo rmmod hello_module
#   dmesg | tail -20

obj-m += hello_module.o

# Kernel build directory (default: running kernel)
KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

.PHONY: all clean install
