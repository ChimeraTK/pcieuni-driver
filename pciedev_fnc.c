#include <linux/types.h>
#include <linux/timer.h>
#include "pciedev_fnc.h"

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
module_dev* pciedev_create_mdev(int brd_num, pciedev_dev* pcidev, unsigned long bufferSize)
{
    module_dev* mdev;
    pciedev_buffer* buffer;
    ushort i;
    
    PDEBUG(pcidev->name, "pciedev_create_mdev(brd_num=%i)", brd_num);
    
    mdev = kzalloc(sizeof(module_dev), GFP_KERNEL);
    if(!mdev) 
    {
        return ERR_PTR(-ENOMEM);
    }
    mdev->brd_num     = brd_num;
    mdev->parent_dev  = pcidev;

    // initalize dma buffer list
    pciedev_bufferList_init(&mdev->dmaBuffers, pcidev); 
    
    // allocate DMA buffers
    for (i = 0; i < 2; i++)
    {
        buffer = pciedev_buffer_create(pcidev, bufferSize);
        if (IS_ERR(buffer)) break;
        pciedev_bufferList_append(&mdev->dmaBuffers, buffer);
    }
    
    if (IS_ERR(buffer))
    {
        pciedev_release_mdev(mdev);
        return ERR_CAST(buffer);
    }
    
    init_waitqueue_head(&mdev->waitDMA);
    sema_init(&mdev->dma_sem, 1);
        
    mdev->waitFlag        = 1;
    mdev->dma_buffer      = 0;
    
    return mdev;
}

void pciedev_release_mdev(module_dev* mdev)
{
    PDEBUG(mdev->parent_dev->name, "pciedev_release_mdev()");
    
    if (!IS_ERR_OR_NULL(mdev))
    {
        // clear the buffers (gracefully)
        pciedev_bufferList_clear(&mdev->dmaBuffers);
        
        // clear the module_dev structure
        kfree(mdev);
    }
}

module_dev* pciedev_get_mdev(struct pciedev_dev *dev)
{
    struct module_dev *mdev = (struct module_dev*)(dev->dev_str);
    return mdev;
}


