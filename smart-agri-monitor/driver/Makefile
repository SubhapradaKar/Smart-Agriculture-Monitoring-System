# Kernel module build. Usage: make  |  make clean
# Needs kernel headers:  sudo apt install build-essential linux-headers-$(uname -r)
obj-m += agri_driver.o
ccflags-y += -I$(src)/../include

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
