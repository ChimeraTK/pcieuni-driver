#include <linux/module.h>
#include <linux/fs.h>   
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/mm.h>

#include "pciedev_fnc.h"
#include "pciedev_buffer.h"

#include "pciedev_fnc.h"
#include "pciedev_buffer.h"

/**
 * @brief Allocates and initializes driver specific data for given pci device
 * 
 * @param brd_num       device index in the list of probed devices
 * @param pcidev        target pci device structure
 * @param kbuf_blk_num  number of preallocated DMA buffers
 * @param kbuf_blk_size size of preallocated DMA buffers
 * 
 * @return  Allocated module_dev strucure
 * @retval  -ENOMEM     Failed - could not allocate memory
 */
module_dev* pciedev_create_drvdata(int brd_num, pciedev_dev* pcidev, unsigned long bufferSize)
{
    module_dev* mdev;
    pciedev_buffer* buffer;
    ushort i;
    
    PDEBUG("pciedev_create_drvdata( brd_num = %i)", brd_num);
    
    mdev = kzalloc(sizeof(module_dev), GFP_KERNEL);
    if(!mdev) 
    {
        return ERR_PTR(-ENOMEM);
    }
    mdev->brd_num     = brd_num;
    mdev->parent_dev  = pcidev;

    // initalize dma buffer ring structure
    pciedev_bufferList_init(&mdev->dmaBuffers, pcidev); 
    
    // allocate DMA buffers
    for (i = 0; i < 2; i++)
    {
        buffer = pciedev_buffer_create(pcidev, bufferSize);
        if (!IS_ERR(buffer))
        {
            pciedev_bufferList_append(&mdev->dmaBuffers, buffer);
        }
        else
        {
            return ERR_CAST(buffer);
        }
        // TODO: see if pciedev_remove gets called automatically
    }
    
    init_waitqueue_head(&mdev->waitDMA);
    sema_init(&mdev->dma_sem, 1);
        
    mdev->waitFlag        = 1;
    mdev->dma_buffer      = 0;
    
    return mdev;
}

void pciedev_release_drvdata(module_dev* mdev)
{
    PDEBUG("pciedev_release_drvdata(mdev = %p)", mdev);
    
    if (!IS_ERR_OR_NULL(mdev))
    {
        // TODO: Graceful shutdown - wait for pending DMA
        
        // TODO Gracefull shutdown: 
        // set shutting down
        // wait until all buffers available (interruptible wait)
        // clear the buffers
        pciedev_bufferList_clear(&mdev->dmaBuffers);
        
        // clear the drvdata structure
        kfree(mdev);
    }
}

module_dev* pciedev_get_moduledata(struct pciedev_dev *dev)
{
    struct module_dev *mdev = (struct module_dev*)(dev->dev_str);
    return mdev;
}


