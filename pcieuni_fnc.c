/**
 *  @file   pcieuni_fnc.c
 *  @brief  Implementation of driver specific utility functions            
 */

#include <linux/sched.h>
#include "pcieuni_fnc.h"

/**
 * @brief Allocates and initializes driver specific part of pci device data
 * 
 * @param brd_num       Device index in the list of probed devices
 * @param pcidev        Universal driver pci device structure
 * @param kbuf_blk_num  Number of preallocated DMA buffers
 * @param kbuf_blk_size Size of preallocated DMA buffers
 * 
 * @return  Allocated module_dev structure
 * @retval  -ENOMEM     Failed - could not allocate memory
 */
module_dev* pcieuni_create_mdev(int brd_num, pcieuni_dev* pcidev, unsigned long bufferSize)
{
    module_dev* mdev;
    pcieuni_buffer* buffer;
    ushort i;
    
    PDEBUG(pcidev->name, "pcieuni_create_mdev(brd_num=%i)", brd_num);
    
#ifdef PCIEUNI_TEST_MDEV_ALLOC_FAILURE
    TEST_RANDOM_EXIT(1, "PCIEUNI: Simulating failed allocation of module_dev structure!", ERR_PTR(-ENOMEM))
#endif   
    
    mdev = kzalloc(sizeof(module_dev), GFP_KERNEL);
    if(!mdev) 
    {
        return ERR_PTR(-ENOMEM);
    }
    mdev->brd_num     = brd_num;
    mdev->parent_dev  = pcidev;

    // initalize dma buffer list
    pcieuni_bufferList_init(&mdev->dmaBuffers, pcidev); 
    
    // allocate DMA buffers
    for (i = 0; i < 2; i++)
    {
        buffer = pcieuni_buffer_create(pcidev, bufferSize);
        if (IS_ERR(buffer)) break;
        pcieuni_bufferList_append(&mdev->dmaBuffers, buffer);
    }
    
    if (IS_ERR(buffer))
    {
        printk(KERN_ERR "PCIEUNI(%s): Failed to allocate DMA buffers!\n", pcidev->name);
        pcieuni_bufferList_clear(&mdev->dmaBuffers);
    }
    
    init_waitqueue_head(&mdev->waitDMA);
    sema_init(&mdev->dma_sem, 1);
        
    mdev->waitFlag        = 1;
    mdev->dma_buffer      = 0;
    
    return mdev;
}

/**
 * @brief Releses driver specific part of pci device data
 * @note This function may block (in case some DMA buffers are in use)
 * 
 * @param mdev   module_dev sructure to be released
 * @return void
 */
void pcieuni_release_mdev(module_dev* mdev)
{
    if (!IS_ERR_OR_NULL(mdev))
    {
        PDEBUG(mdev->parent_dev->name, "pcieuni_release_mdev()");
        
        // clear the buffers gracefully
        pcieuni_bufferList_clear(&mdev->dmaBuffers);
        
        // clear the module_dev structure
        kfree(mdev);
    }
}

/**
 * @brief Returns driver specific part of pci device data
 * 
 * @param   dev     Universal driver pci device structure
 * @return  module_dev structure
 */
module_dev* pcieuni_get_mdev(struct pcieuni_dev *dev)
{
    struct module_dev *mdev = (struct module_dev*)(dev->dev_str);
    return mdev;
}

/**
 * @brief Reserves DMA read process on target device
 * 
 * @note This function will block until device is ready to acceept DMA request. 
 * 
 * @param   mdev   PCI device 
 * @param   buffer Target DMA buffer
 * 
 * @retval  0      Success
 * @retval  -EINTR Operation was interrupted
 * @retval  -EBUSY Operation timed out
 */
int pcieuni_dma_reserve(module_dev* mdev, pcieuni_buffer* buffer)
{
    long waitVal = 0;
    long timeout = HZ/1;
    
    PDEBUG(mdev->parent_dev->name, "pcieuni_dma_reserve()");
   
    // protect against concurrent reservation of waitFlag
    if (down_interruptible(&mdev->dma_sem))
    {
        return -EINTR;
    }
    
    while (!mdev->waitFlag)
    {
        // wait for DMA to be available
        up(&mdev->dma_sem);
        
        PDEBUG(mdev->parent_dev->name, "pcieuni_dma_reserve(): Waiting until dma available...\n"); 
        waitVal = wait_event_interruptible_timeout(mdev->waitDMA, mdev->waitFlag, timeout);
        if (0 == waitVal)
        {
            PDEBUG(mdev->parent_dev->name, "pcieuni_dma_reserve(): Timeout!\n"); 
            return -EBUSY; 
        }
        else if (0 > waitVal)
        {
            PDEBUG(mdev->parent_dev->name, "pcieuni_dma_reserve(): Interrupted!\n"); 
            return -EINTR;
        }
        
#ifdef PCIEUNI_TEST_DEVICE_DMA_BLOCKED
        TEST_RANDOM_EXIT(100, "PCIEUNI: Simulating blocked DMA !", -EBUSY)
#endif
        
        // protect against concurrent reservation of waitFlag
        if (down_interruptible(&mdev->dma_sem))
        {
            PDEBUG(mdev->parent_dev->name, "pcieuni_dma_reserve(): Interrupted!\n"); 
            return -EINTR;
        }        
    }
    
    // Mark DMA read operation reserved and set the target buffer
    mdev->waitFlag   = 0;
    mdev->dma_buffer = buffer; 
    up(&mdev->dma_sem);
    
    return 0;
}

/**
 * @brief Releases DMA read process on target device
 * 
 * @note This function is called from interrupt handler so it must not block.
 * 
 * @param mdev  Driver device structure
 */
void pcieuni_dma_release(module_dev* mdev)
{
    PDEBUG(mdev->parent_dev->name, "pcieuni_dma_release()");
    mdev->waitFlag   = 1;
    mdev->dma_buffer = 0; 
    wake_up_interruptible(&(mdev->waitDMA));
}


