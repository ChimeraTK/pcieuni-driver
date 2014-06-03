/**
 *  @file   pciedev_fnc.h
 *  @brief  Driver specific definitions           
 */

#ifndef _PCIEDEV_FNC_H_
#define _PCIEDEV_FNC_H_

#include <linux/semaphore.h>

#include "pciedev_io.h"
#include "pciedev_ufn.h"
#include "pciedev_buffer.h"

#define DEVNAME "pciedev"	                             /* name of device */
#define PCIEDEV_VENDOR_ID 0x10EE	                    /* XILINX vendor ID */
#define PCIEDEV_DEVICE_ID 0x0088	                    /* PCIEDEV dev board device ID or 0x0088 */
#define PCIEDEV_SUBVENDOR_ID PCI_ANY_ID	 /* ESDADIO vendor ID */
#define PCIEDEV_SUBDEVICE_ID PCI_ANY_ID           /* ESDADIO device ID */

#define DMA_BOARD_ADDRESS   0x4
#define DMA_CPU_ADDRESS     0x8
#define DMA_SIZE_ADDRESS    0xC

/**
 * @brief Driver specific part of PCI device structure
 */
struct module_dev {
    int                brd_num;            /**< PCI board number */
    
    struct pciedev_buffer_list dmaBuffers; /**< List of preallocated DMA buffers */
    
    struct timeval     dma_start_time;
    struct timeval     dma_stop_time;
    int                waitFlag;           /**< Locks access to PCI device DMA read process */
    wait_queue_head_t  waitDMA;            /**< Wait queue for DMA read process to finish  */
    struct semaphore   dma_sem;            /**< Semaphore that protects against concurrent acquisition DMA read process */
    pciedev_buffer     *dma_buffer;        /**< DMA buffer used by current/last DMA read process */
    
    struct pciedev_dev *parent_dev;        /**< Universal driver part of the parent PCI device structure */
};
typedef struct module_dev module_dev;

/* Driver specific utility functions */ 
module_dev* pciedev_create_mdev(int brd_num, pciedev_dev* pcidev, unsigned long bufferSize);
void pciedev_release_mdev(module_dev* mdev);
module_dev* pciedev_get_mdev(struct pciedev_dev *dev);

long pciedev_ioctl_dma(struct file *, unsigned int* , unsigned long*, pciedev_cdev * );

int  pciedev_dma_reserve(module_dev* dev, pciedev_buffer* buffer);
void pciedev_dma_release(module_dev* mdev);



#endif /* _PCIEDEV_FNC_H_ */
