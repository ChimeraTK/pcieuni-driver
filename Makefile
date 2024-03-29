pcieuni-objs := pcieuni_drv.o pcieuni_fnc.o pcieuni_ioctl_dma.o
obj-m := pcieuni.o

KVERSION = $(shell uname -r)

#define the package/module version (the same for this driver)
PCIEUNI_PACKAGE_VERSION=0.1.11

PCIEUNI_DKMS_SOURCE_DIR=/usr/src/pcieuni-${PCIEUNI_PACKAGE_VERSION}

ccflags-y = -Wall -Wuninitialized

all: configure-source-files
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) modules

#Performs a dkms install
install: dkms-prepare
	dkms install -m pcieuni -v ${PCIEUNI_PACKAGE_VERSION} -k $(KVERSION)

#Performs a dmks remove
#Always returns true, so purge also works if there is no driver installed
uninstall:
	dkms remove -m pcieuni -v ${PCIEUNI_PACKAGE_VERSION} -k $(KVERSION) || true

#Compile with debug flag, causes lots of kernel output, which fills the logs
#and slows down the driver.
debug:
	KCPPFLAGS="-DPCIEUNI_DEBUG" make all

#In addition to the debug output the driver is compiled with code coverage.
#It only loads on a kernel with code coverage enabled.
full_debug:
	KCPPFLAGS="-DPCIEUNI_DEBUG -fprofile-arcs -ftest-coverage" make all

clean:
	make -C /lib/modules/$(KVERSION)/build V=1 M=$(PWD) clean
	rm -f pcieuni_drv.c

#uninstall and
purge: uninstall
	rm -rf ${PCIEUNI_DKMS_SOURCE_DIR} /etc/udev/rules.d/10-pcieuni.rules /lib/udev/pcieuni-symlinks

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
	(cd debian.in; cp compat  control  copyright ../debian_from_template)
	cat debian.in/rules.in | sed "{s/@PCIEUNI_PACKAGE_VERSION@/${PCIEUNI_PACKAGE_VERSION}/}" > debian_from_template/rules
	chmod +x debian_from_template/rules

#copies the package sources to the place needed by dkms
#The udev rules also have to be placed, so they are available for all kernels and not uninstalled if
# the module for one kernel is removed.
dkms-prepare: configure-source-files configure-package-files
	test -d ${PCIEUNI_DKMS_SOURCE_DIR} || mkdir ${PCIEUNI_DKMS_SOURCE_DIR}
	cp *.h *.c pcieuni_drv.c.in Makefile dkms.conf *.rules ${PCIEUNI_DKMS_SOURCE_DIR}
	install --mode=644 *.rules /etc/udev/rules.d
	install --mode=755 pcieuni-symlinks /lib/udev
