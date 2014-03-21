#ifndef _PCIEDEV_FNC_H_
#define _PCIEDEV_FNC_H_

#include "pciedev_io.h"
#include "pciedev_ufn.h"

#define DEVNAME "pciedev"	                             /* name of device */
#define PCIEDEV_VENDOR_ID 0x10EE	                    /* XILINX vendor ID */
#define PCIEDEV_DEVICE_ID 0x0088	                    /* PCIEDEV dev board device ID or 0x0088 */
#define PCIEDEV_SUBVENDOR_ID PCI_ANY_ID	 /* ESDADIO vendor ID */
#define PCIEDEV_SUBDEVICE_ID PCI_ANY_ID           /* ESDADIO device ID */

#define DMA_BOARD_ADDRESS     0x4
#define DMA_CPU_ADDRESS          0x8
#define DMA_SIZE_ADDRESS          0xC

long     pciedev_ioctl_dma(struct file *, unsigned int* , unsigned long*, pciedev_cdev * );

#endif /* _PCIEDEV_FNC_H_ */
