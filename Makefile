pcieuni-objs := pcieuni_drv.o pcieuni_fnc.o pcieuni_ioctl_dma.o 
obj-m := pcieuni.o 

KVERSION = $(shell uname -r)

#define the package/module version (the same for this driver)
PCIEUNI_PACKAGE_VERSION=0.1.0

all: configure-source-files
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules

install: all
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules_install
	cp 10-pcieuni.rules /etc/udev/rules.d
	depmod

debug:
	KCPPFLAGS="-DPCIEUNI_DEBUG" make all

clean:
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) clean
	rm -f pcieuni_drv.c

#the two possible locations where the gpcieuni include files can be
#/usr/local in case of a local, manual installation by the admin
#/usr in case of an installation by the package management
EXTRA_CFLAGS += -I/usr/local/include -I/usr/include

KBUILD_EXTRA_SYMBOLS = /lib/modules/$(KVERSION)/gpcieuni/Module.symvers

##### Internal targets usually not called by the user: #####

#A target which replaces the version number in the source files
configure-source-files:
	cat pcieuni_drv.c.in | sed "{s/@PCIEUNI_PACKAGE_VERSION@/${PCIEUNI_PACKAGE_VERSION}/}" > pcieuni_drv.c
