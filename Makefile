pcieuni-objs := pcieuni_drv.o pcieuni_fnc.o pcieuni_ioctl_dma.o 
	
obj-m := pcieuni.o 
#pcieuni-y := pcieuni_ufn.o pcieuni.o

KVERSION = $(shell uname -r)
all:
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules

debug:
	KCPPFLAGS="-DPCIEUNI_DEBUG" make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules

clean:
	test ! -d /lib/modules/$(KVERSION) || make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) clean

EXTRA_CFLAGS += -I/usr/include -I/usr/local/include/gpcieuni

#KBUILD_EXTRA_SYMBOLS = /home/petros/doocs/source/unixdriver/utca/linux/gpcieuni*/Module.symvers
KBUILD_EXTRA_SYMBOLS = /lib/modules/$(KVERSION)/gpcieuni/Gpcieuni.symvers