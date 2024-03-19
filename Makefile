obj-m += my_loop_device.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
CC := gcc-12

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

