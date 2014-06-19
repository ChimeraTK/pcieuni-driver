pciedev-objs := pciedev_drv.o pciedev_fnc.o pciedev_ioctl_dma.o 
	
obj-m := pciedev.o 
#pciedev-y := pciedev_ufn.o pciedev.o

KVERSION = $(shell uname -r)
all:
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules

debug:
	KCPPFLAGS="-DPCIEDEV_DEBUG" make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules

clean:
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) clean

EXTRA_CFLAGS += -I/usr/include -I/usr/local/include/upciedev

#KBUILD_EXTRA_SYMBOLS = /home/petros/doocs/source/unixdriver/utca/linux/upciedev*/Module.symvers
KBUILD_EXTRA_SYMBOLS = /lib/modules/$(KVERSION)/upciedev/Upciedev.symvers