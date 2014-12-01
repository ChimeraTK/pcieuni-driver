pcieuni-objs := pcieuni_drv.o pcieuni_fnc.o pcieuni_ioctl_dma.o 
obj-m := pcieuni.o 

KVERSION = $(shell uname -r)

#define the package/module version (the same for this driver)
PCIEUNI_PACKAGE_VERSION=0.1.0

all: configure-source-files
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules

#Performs a dkms install
install: dkms-prepare
	dkms install -m pcieuni -v ${PCIEUNI_PACKAGE_VERSION} -k $(KVERSION)

#Performs a dmks remove
uninstall:
	dkms remove -m pcieuni -v ${PCIEUNI_PACKAGE_VERSION} -k $(KVERSION)

#compile with debug flag, causes lots of kernel output
debug:
	KCPPFLAGS="-DPCIEUNI_DEBUG" make all

clean:
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) clean
	rm -f pcieuni_drv.c

#This target will only succeed on debian machines with the debian packaging tools installed
debian_package: configure-package-files
	./make_debian_package.sh ${PCIEUNI_PACKAGE_VERSION}

#the two possible locations where the gpcieuni include files can be
#/usr/local in case of a local, manual installation by the admin
#/usr in case of an installation by the package management
EXTRA_CFLAGS += -I/usr/local/include -I/usr/include

KBUILD_EXTRA_SYMBOLS = /lib/modules/$(KVERSION)/gpcieuni/Module.symvers

##### Internal targets usually not called by the user: #####

#A target which replaces the version number in the source files
configure-source-files:
	cat pcieuni_drv.c.in | sed "{s/@PCIEUNI_PACKAGE_VERSION@/${PCIEUNI_PACKAGE_VERSION}/}" > pcieuni_drv.c

#A target which replaces the version number in the control files for
#dkms and debian packaging
configure-package-files:
	cat dkms.conf.in | sed "{s/@PCIEUNI_PACKAGE_VERSION@/${PCIEUNI_PACKAGE_VERSION}/}" > dkms.conf
	test -d debian_from_template || mkdir debian_from_template
	cp dkms.conf debian_from_template/pcieuni-dkms.dkms
	cp dkms.post_* debian_from_template/
	(cd debian.in; cp compat  control  copyright ../debian_from_template)
	cat debian.in/rules.in | sed "{s/@PCIEUNI_PACKAGE_VERSION@/${PCIEUNI_PACKAGE_VERSION}/}" > debian_from_template/rules
	chmod +x debian_from_template/rules

#copies the package sources to the place needed by dkms
dkms-prepare: configure-source-files configure-package-files
	test -d /usr/src/pcieuni-${PCIEUNI_PACKAGE_VERSION} || mkdir /usr/src/pcieuni-${PCIEUNI_PACKAGE_VERSION}
	cp *.h *.c pcieuni_drv.c.in Makefile dkms.conf dkms.post_* *.rules /usr/src/pcieuni-${PCIEUNI_PACKAGE_VERSION}
