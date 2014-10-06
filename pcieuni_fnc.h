/**
 *  @file   pcieuni_fnc.h
 *  @brief  Driver specific definitions           
 */

#ifndef _PCIEUNI_FNC_H_
#define _PCIEUNI_FNC_H_

#include <linux/semaphore.h>

#include "pcieuni_io.h"
#include "pcieuni_ufn.h"
#include "pcieuni_buffer.h"

#define DEVNAME "pcieuni"	                             /* name of device */
#define PCIEUNI_VENDOR_ID 0x10EE	                    /* XILINX vendor ID */
#define PCIEUNI_DEVICE_ID 0x0088	                    /* PCIEUNI dev board device ID or 0x0088 */
#define PCIEUNI_SUBVENDOR_ID PCI_ANY_ID	 /* ESDADIO vendor ID */
#define PCIEUNI_SUBDEVICE_ID PCI_ANY_ID           /* ESDADIO device ID */

#define DMA_BOARD_ADDRESS   0x4
#define DMA_CPU_ADDRESS     0x8
#define DMA_SIZE_ADDRESS    0xC

/**
 * @brief Driver specific part of PCI device structure
 */
struct module_dev {
    int                brd_num;            /**< PCI board number */
    
    struct pcieuni_buffer_list dmaBuffers; /**< List of preallocated DMA buffers */
    
    struct timeval     dma_start_time;
    struct timeval     dma_stop_time;
    int                waitFlag;           /**< Locks access to PCI device DMA read process */
    wait_queue_head_t  waitDMA;            /**< Wait queue for DMA read process to finish  */
    struct semaphore   dma_sem;            /**< Semaphore that protects against concurrent acquisition DMA read process */
    pcieuni_buffer     *dma_buffer;        /**< DMA buffer used by current/last DMA read process */
    
    struct pcieuni_dev *parent_dev;        /**< Universal driver part of the parent PCI device structure */
};
typedef struct module_dev module_dev;

/* Driver specific utility functions */ 
module_dev* pcieuni_create_mdev(int brd_num, pcieuni_dev* pcidev, unsigned long bufferSize);
void pcieuni_release_mdev(module_dev* mdev);
module_dev* pcieuni_get_mdev(struct pcieuni_dev *dev);

long pcieuni_ioctl_dma(struct file *, unsigned int* , unsigned long*, pcieuni_cdev * );

int  pcieuni_dma_reserve(module_dev* dev, pcieuni_buffer* buffer);
void pcieuni_dma_release(module_dev* mdev);



#endif /* _PCIEUNI_FNC_H_ */
