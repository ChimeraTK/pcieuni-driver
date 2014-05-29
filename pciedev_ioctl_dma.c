#include <linux/types.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/kernel.h>
#include <linux/wait.h>

#include "pciedev_fnc.h"
#include "pciedev_io.h"
#include "pciedev_ufn.h"
#include "pciedev_buffer.h"

/**
 * @brief Reserve device for DMA to traget buffer
 * 
 * This function will block until device is ready to acceept DMA request. 
 * 
 * @param   mdev   Target device structure
 * @param   buffer Target buffer
 * 
 * @retval  0      Success
 * @retval  -EINTR Operation was interrupted
 * @retval  -EBUSY Operation timed out
 */
int pciedev_dma_reserve(module_dev* mdev, pciedev_buffer* buffer)
{
    long waitVal = 0;
    long timeout = HZ/1;
    
    PDEBUG("pciedev_dma_reserve(dev.name=%s)", mdev->parent_dev->name);
    
    if (down_interruptible(&mdev->dma_sem))
    {
        return -EINTR;
    }

    // wait for DMA to be available
    while (!mdev->waitFlag)
    {
        up(&mdev->dma_sem);

        PDEBUG("pciedev_dma_reserve(dev.name=%s): Waiting until dma available...\n", mdev->parent_dev->name); 
        waitVal = wait_event_interruptible_timeout(mdev->waitDMA, mdev->waitFlag, timeout);
        if (0 == waitVal)
        {
            PDEBUG("pciedev_dma_reserve(dev.name=%s): Timeout!\n", mdev->parent_dev->name); 
            return -EBUSY; 
        }
        else if (0 > waitVal)
        {
            PDEBUG("pciedev_dma_reserve(dev.name=%s): Interrupted!\n", mdev->parent_dev->name); 
            return -EINTR;
        }
        
        if (down_interruptible(&mdev->dma_sem))
        {
            PDEBUG("pciedev_dma_reserve(dev.name=%s): Interrupted!\n", mdev->parent_dev->name); 
            return -EINTR;
        }        
    }
    
    mdev->waitFlag   = 0;
    mdev->dma_buffer = buffer; 
    up(&mdev->dma_sem);
    
    return 0;
}

/**
 * @brief Release device which was reserved for DMA
 * 
 * @note This function is called from interrupt handler so it must not block.
 * 
 * @param mdev  Driver device structure
 */
void pciedev_dma_release(module_dev* mdev)
{
    PDEBUG("pciedev_dma_release(dev.name=%s)", mdev->parent_dev->name);
    mdev->waitFlag   = 1;
    mdev->dma_buffer = 0; 
    wake_up_interruptible(&(mdev->waitDMA));
}

/**
 * @brief Wait until DMA read to target buffer is finished
 * 
 * @param mdev   Target driver device structure
 * @param buffer Target driver buffer
 * 
 * @retval 0            Success
 * @retval -EIO         Timed out while waiting for end of DMA IRQ
 * @retval -EINTR       Interrupted while waiting for end of DMA IRQ
 */
int pciedev_wait_dma_read(module_dev* mdev, pciedev_buffer* buffer)
{
    int   code;
    ulong timeout = HZ/1; // Timeout in 1 second
    
    PDEBUG("pciedev_wait_dma_read(dev.name=%s, offset=0x%lx, size=0x%lx)", mdev->parent_dev->name, buffer->dma_offset, buffer->dma_size);
    while(test_bit(BUFFER_STATE_WAITING, &buffer->state))
    {
        // DMA not finished yet - wait for IRQ handler 
        PDEBUG("pciedev_wait_dma_read(dev.name=%s, offset=0x%lx, size=0x%lx): Waiting... \n",  mdev->parent_dev->name, buffer->dma_offset, buffer->dma_size); 
        
        code = wait_event_interruptible_timeout( mdev->waitDMA, !test_bit(BUFFER_STATE_WAITING, &buffer->state), timeout);
        if (code == 0)
        {
            printk(KERN_ALERT "PCIEDEV(%s): Error waiting for DMA to buffer (offset=0x%lx, size=0x%lx): TIMEOUT!\n", 
                   mdev->parent_dev->name, buffer->dma_offset, buffer->dma_size);

            pciedev_dma_release(mdev); // TODO: disable/enable interrupts?
            return -EIO; 
        }
        else if (code < 0)
        {
            printk(KERN_ALERT "PCIEDEV(%s): Error waiting for DMA to buffer (offset=0x%lx, size=0x%lx): errno=%d!\n", 
                   mdev->parent_dev->name, buffer->dma_offset, buffer->dma_size, code);
            
            pciedev_dma_release(mdev); // TODO: disable/enable interrupts?
            return -EINTR; 
        }
    }
    
    dma_sync_single_for_cpu(&mdev->parent_dev->pciedev_pci_dev->dev, buffer->dma_handle, (size_t)buffer->size, DMA_FROM_DEVICE);
    PDEBUG("pciedev_wait_dma_read(dev.name=%s, offset=0x%lx, size=0x%lx): Done!", mdev->parent_dev->name, buffer->dma_offset, buffer->dma_size);
    
    do_gettimeofday(&(mdev->dma_stop_time));
    return 0;
}

        
/**
 * @brief Request DMA read from device
 * 
 * This function will find available driver buffer and initiate DMA read from device into the buffer. In case there is no 
 * avaialble buffer or in case the device is busy the fuction will block. Once the DMA transfer is initiated the function 
 * will return immediately.
 * @note Size of requested data can only be as big as the target buffer. Caller should check the dma_size field of returned 
 *       buffer to see how much data will be read. 
 * 
 * @param dev        Traget device structure
 * @param dmaOffset  Offset in device memeory
 * @param size       Size of data to be read
 * 
 * @return           Driver buffer used for DMA transfer
 * @retval   -ENOMEM Failed to get target driver buffer          
 * @retval   -EBUSY  Cannot initiate DMA because target device is busy          
 * @retval   -EINTR  Operation was interupted
 * @retval   -EIO    Failed to write to device registers
 */
pciedev_buffer *pciedev_start_dma_read(pciedev_dev* dev, unsigned long dmaOffset, unsigned long size)
{
    struct module_dev *mdev = pciedev_get_moduledata(dev);
    pciedev_buffer *targetBuffer = 0;
    int retVal = 0;
    
    PDEBUG("pciedev_start_dma(dev.name= %s, offset=0x%lx, maxSize=0x%lx)\n",  dev->name, dmaOffset, size); 
    
    // Find and reserve target buffer
    targetBuffer = pciedev_buffer_get_free(mdev);
    if (IS_ERR(targetBuffer)) 
    {
        return (targetBuffer == ERR_PTR(-EINTR)) ? targetBuffer : ERR_PTR(-ENOMEM);
    }
    targetBuffer->dma_size   = min(size, targetBuffer->size);
    targetBuffer->dma_offset = dmaOffset; 

    // prepare buffer to accept DMA data from device
    dma_sync_single_for_device(&dev->pciedev_pci_dev->dev, targetBuffer->dma_handle, (size_t)targetBuffer->size, DMA_FROM_DEVICE);
    
    // reserve device registers IO
    retVal = pciedev_dma_reserve(mdev, targetBuffer);
    if (retVal) goto cleanup_releaseBuffer;
    
    // write DMA source address to device register
    retVal = pciedev_register_write32(targetBuffer->dma_offset, (void*)(dev->memmory_base2 + DMA_BOARD_ADDRESS), false);
    if (retVal) goto cleanup_releaseDevice;
    
    // write DMA destination address to device register
    retVal = pciedev_register_write32((u32)(targetBuffer->dma_handle & 0xFFFFFFFF), (void*)(dev->memmory_base2 + DMA_CPU_ADDRESS), true);
    if (retVal) goto cleanup_releaseDevice; 
    
    do_gettimeofday(&(mdev->dma_start_time));

    // Setup env for irq handler
    mdev->waitFlag   = 0;
    mdev->dma_buffer = targetBuffer; 
    
    // write DMA size and start DMA
    retVal = pciedev_register_write32(targetBuffer->dma_size, (void*)(dev->memmory_base2 + DMA_SIZE_ADDRESS ), false);
    if (retVal) goto cleanup_releaseDevice; 
    
    PDEBUG("pciedev_start_dma(dev.name=%s): DMA started, offset=0x%lx, size=0x%lx \n", dev->name, targetBuffer->dma_offset, targetBuffer->dma_size); 
    
    
cleanup_releaseDevice:
    if (retVal)
    {
        // release device registers IO
        pciedev_dma_release(mdev);
    }

cleanup_releaseBuffer:
    if (retVal)
    {
        // make buffer available for next DMA request
        dma_sync_single_for_cpu(&dev->pciedev_pci_dev->dev, targetBuffer->dma_handle, (size_t)targetBuffer->size, DMA_FROM_DEVICE);
        pciedev_buffer_set_free(mdev, targetBuffer);
    }
    
    return retVal ? ERR_PTR(retVal) : targetBuffer;
}

/**
 * @brief Read from board memory via DMA using driver allocated buffers
 * 
 * @param dev         Target device structure
 * @param devOffset   Offset in device memory
 * @param dataSize    Size of data to be read 
 * @param userBuffer  Target user-space buffer
 * 
 * @retval  0          Success
 * @retval  -EFAULT    Failed to copy data to userspace
 * @retval  -ENOMEM    Failed to get target driver buffer          
 * @retval  -EBUSY     Cannot initiate DMA because target device is busy          
 * @retval  -EINTR     Operation was interupted
 * @retval  -EIO       Failed to write to device registers
 * @retval  -EIO       Timed out while waiting for end of DMA IRQ from device
 */
int pciedev_dma_read(pciedev_dev* dev, unsigned long devOffset, unsigned long dataSize, void* userBuffer)
{
    int retVal = 0;
    unsigned long dmaSize   = PCIEDEV_DMA_SYZE * DIV_ROUND_UP(dataSize, PCIEDEV_DMA_SYZE); // round up total read-size to page boundary
    unsigned long dataReq   = 0;  // Total size of data that was requested from device
    unsigned long dataRead  = 0;  // Total size of data read from device
    pciedev_buffer* prevBuffer = 0; // buffer used for read in previous loop
    pciedev_buffer* nextBuffer = 0; // buffer to read to in this loop
    struct module_dev* mdev  = pciedev_get_moduledata(dev);
    
    PDEBUG("pciedev_dma_read(dev.name=%s, devOffset=0x%lx, dataSize=0x%lx)\n",  dev->name, devOffset, dataSize); 
    
    // Loop until data is read 
    for (; !IS_ERR(prevBuffer) && (dataRead < dmaSize); )
    {
        if (retVal) 
        {
            nextBuffer = ERR_PTR(retVal);
        }
        else
        {
            // if there is more data to be requested from device
            if (dataReq < dmaSize)
            {
                // request read of next data chunk
                nextBuffer = pciedev_start_dma_read(dev, devOffset + dataReq, dmaSize - dataReq);
                if (!IS_ERR(nextBuffer))
                {
                    dataReq += nextBuffer->dma_size; // add to total data requested
                }
            }
            else
            {
                nextBuffer = 0;
            }
        }
        
        // if data read was requested for prevBuffer
        if (prevBuffer)
        {
            // wait until data read is completed (device irq)
            retVal = pciedev_wait_dma_read(mdev, prevBuffer); 
            if (!retVal)
            {
                // copy data to proper offset in the target user-space buffer 
                if (copy_to_user(userBuffer + dataRead, (void*) prevBuffer->kaddr, min(prevBuffer->dma_size, dataSize - dataRead)))
                {
                    retVal = -EFAULT;
                }
                else
                {
                    // add to total data read
                    dataRead += prevBuffer->dma_size; 
                }
            }
            // mark buffer available
            pciedev_buffer_set_free(mdev, prevBuffer); 
        }
        
        prevBuffer = nextBuffer;
    }
    
    if (IS_ERR(prevBuffer))
    {
        retVal = retVal ? retVal : PTR_ERR(prevBuffer);
    }
    
    PDEBUG("pciedev_dma_read(dev.name=%s, devOffset=0x%lx, dataSize=0x%lx): Return code(%i)\n",  dev->name, devOffset, dataSize, retVal); 
    
    return retVal;
}



long pciedev_ioctl_dma(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, pciedev_cdev * pciedev_cdev_m)
{
    unsigned int    cmd;
    unsigned long arg;
    pid_t           cur_proc = 0;
    int             minor    = 0;
    int             d_num    = 0;
    int             retval   = 0;
    int             err      = 0;
    struct pci_dev* pdev;

    int size_time;
    int io_dma_size;
    device_ioctrl_time  time_data;
    device_ioctrl_dma   dma_data;
    
    module_dev       *module_dev_pp;
    pciedev_dev       *dev  = filp->private_data;
    module_dev_pp = pciedev_get_moduledata(dev);
    
    cmd         = *cmd_p;
    arg         = *arg_p;
    size_time   = sizeof(device_ioctrl_time);
    io_dma_size = sizeof(device_ioctrl_dma);
    minor       = dev->dev_minor;
    d_num       = dev->dev_num;	
    cur_proc    = current->group_leader->pid;
    pdev        = (dev->pciedev_pci_dev);

    
    if(!dev->dev_sts){
        printk(KERN_ALERT "PCIEDEV_IOCTRL: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
    
    PDEBUG("pciedev_ioctl_dma(dev.name=%s, nr=%d )", dev->name, _IOC_NR(cmd));
    
    /*
    * the direction is a bitmask, and VERIFY_WRITE catches R/W
    * transfers. `Type' is user-oriented, while
    * access_ok is kernel-oriented, so the concept of "read" and
    * "write" is reversed
    */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    
    if (err) return -EFAULT;

    if (mutex_lock_interruptible(&dev->dev_mut))
        return -ERESTARTSYS;

    switch (cmd) {
        case PCIEDEV_GET_DMA_TIME:
            retval = 0;
            
            module_dev_pp->dma_start_time.tv_sec  += (long)dev->slot_num;
            module_dev_pp->dma_stop_time.tv_sec   += (long)dev->slot_num;
            module_dev_pp->dma_start_time.tv_usec += (long)dev->brd_num;
            module_dev_pp->dma_stop_time.tv_usec  += (long)dev->brd_num;
            time_data.start_time = module_dev_pp->dma_start_time;
            time_data.stop_time  = module_dev_pp->dma_stop_time;
            if (copy_to_user((device_ioctrl_time*)arg, &time_data, (size_t)size_time)) {
                retval = -EIO;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
            
        case PCIEDEV_KBUF_INFO:
        {   
            device_ioctrl_kbuf_info info;
            info.num_blocks = 0;
            info.block_size = 0;
            
            if (!list_empty(&module_dev_pp->dma_bufferList))
            {
                pciedev_buffer* block = list_first_entry(&module_dev_pp->dma_bufferList, struct pciedev_buffer, list);
                info.block_size = block->size;
                
                list_for_each_entry(block, &module_dev_pp->dma_bufferList, list)
                {
                    info.block_size = min(info.block_size, block->size);
                    info.num_blocks++;
                }
            }
            
            if (copy_to_user((device_ioctrl_kbuf_info*)arg, &info, sizeof(device_ioctrl_kbuf_info))) 
            {
                retval = -EIO;
            }
            break;
        }
                
        case PCIEDEV_READ_DMA:
        {
            // Copy DMA transfer arguments into workqeue-data structure 
            if (copy_from_user(&dma_data, (void*)arg, io_dma_size)) {
                mutex_unlock(&dev->dev_mut);
                return -EFAULT;
            }
            
            retval = pciedev_dma_read(dev, dma_data.dma_offset, dma_data.dma_size, (void*)arg);
            break;
        }   
        
        default:
            return -ENOTTY;
            break;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;
}

