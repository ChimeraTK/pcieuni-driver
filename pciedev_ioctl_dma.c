#include <linux/types.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/wait.h>

#include "pciedev_dma.h"
#include "pciedev_fnc.h"
#include "pciedev_io.h"
#include "pciedev_ufn.h"
#include "pciedev_buffer.h"





// Device handling code
int pciedev_dma_reserve(module_dev* dev)
{
    ulong timeout = HZ/1;
    
    PDEBUG("pciedev_dma_reserve(dev=0x%lx)", dev);
    
    if (down_interruptible(&dev->dma_sem))
    {
        return -ERESTARTSYS; // TODO: check return code
    }

    // wait for DMA to be available
    while (!dev->waitFlag)
    {
        up(&dev->dma_sem);
        // TODO: handle timeout
        PDEBUG("pciedev_dma_reserve: Waiting until dma available\n"); 
        if (0 >= wait_event_interruptible_timeout(dev->waitDMA, dev->waitFlag != 0, timeout))
        {
            return -ERESTARTSYS; // TODO: check return code
        }
        
        if (down_interruptible(&dev->dma_sem))
        {
            return -ERESTARTSYS; // TODO: check return code
        }        
    }
    
    return 0;
}

void pciedev_start_dma_read(module_dev* mdev, void* bar, pciedev_buffer* block)
{
    u32 tmp_data_32;
    
    PDEBUG("pciedev_start_dma_read(dma_offset=0x%lx, dma_size=0x%lx, drv_offset=0x%lx)\n", block->dma_offset, block->dma_size, block->offset); 
    //print_hex_dump_bytes("Buffer contents before DMA: ", DUMP_PREFIX_NONE, block->kaddr, 64);
    
    dma_sync_single_for_device(&mdev->parent_dev->pciedev_pci_dev->dev, block->dma_handle, (size_t)block->size, DMA_FROM_DEVICE);
    
    iowrite32(block->dma_offset,                     (void*)(bar + DMA_BOARD_ADDRESS));
    iowrite32((u32)(block->dma_handle & 0xFFFFFFFF), (void*)(bar + DMA_CPU_ADDRESS  ));
    
    smp_wmb();
    // TODO: ask if delay and/or read are really neccessary here
    //udelay(5);
    tmp_data_32       = ioread32(bar + 0x0); // be safe all writes are done
    smp_rmb();
    
    do_gettimeofday(&(mdev->dma_start_time));
    
    mdev->waitFlag = 0;
    
    mdev->dma_buffer = block; 
    //start DMA
    // TODO: switch back
    iowrite32(block->dma_size,                       (void*)(bar + DMA_SIZE_ADDRESS ));
    //iowrite32(0,                       (void*)(dma->reg_address + DMA_SIZE_ADDRESS ));
}


int pciedev_dma_release(module_dev* mdev)
{
    PDEBUG("pciedev_dma_release(mdev=0x%lx)", mdev);
    
    up(&mdev->dma_sem);
    return 0;
}


// dma algorithm


int pciedev_wait_dma_read(module_dev* dev, pciedev_buffer* block)
{
    ulong timeout = HZ/1;

    PDEBUG("pciedev_wait_dma_read(drv_offset=0x%lx, size=0x%lx)\n",  block->offset, block->size); 
    while(!block->dma_done)
    {
        PDEBUG("pciedev_wait_dma_read(drv_offset=0x%lx, size=0x%lx): Waiting... \n",  block->offset, block->size); 
        
        int code = wait_event_interruptible_timeout( dev->waitDMA, block->dma_done, timeout);
        if (code == 0)
        {
            printk(KERN_ALERT "PCIEDEV: Error waiting for DMA to buffer (dma_offset=0x%lx, dma_size=0x%lx, drv_offset==0x%lx): TIMEOUT!\n", block->dma_offset, block->dma_size, block->offset);            
            return -ERESTARTSYS; // TODO: check return code
        }
        else if (code < 0)
        {
            printk(KERN_ALERT "PCIEDEV: Error waiting for DMA to buffer (dma_offset=0x%lx, dma_size=0x%lx, drv_offset==0x%lx): errno=%d!\n", block->dma_offset, block->dma_size, block->offset, code);            
            return -ERESTARTSYS; // TODO: check return code   
        }
    }
    
    PDEBUG("pciedev_wait_dma_read(drv_offset=0x%lx, size=0x%lx): Done! \n",  block->offset, block->size);
    dma_sync_single_for_cpu(&dev->parent_dev->pciedev_pci_dev->dev, block->dma_handle, (size_t)block->size, DMA_FROM_DEVICE);
    
    do_gettimeofday(&(dev->dma_stop_time));
    return 0;
}




        
pciedev_buffer *pciedev_start_dma(pciedev_dev* dev, u_int dmaOffset, u_int size)
{
    struct module_dev *mdev = (struct module_dev*)(dev->dev_str);
    pciedev_buffer *targetBlock = 0;
    
    PDEBUG("pciedev_start_dma(dmaOffset=0x%lx, size=0x%lx)\n",  dmaOffset, size); 
    
    targetBlock = pciedev_buffer_get_free(mdev);
    if(!targetBlock) 
    {
        // TODO: handle error properly
        return ERR_PTR(-ENOMEM);
    }
    
    targetBlock->dma_size   = min(size, targetBlock->size);
    targetBlock->dma_offset = dmaOffset; 
    
    if (pciedev_dma_reserve(mdev)) 
    {
        // TODO: handle error properly
        return ERR_PTR(-EBUSY);
    }
    pciedev_start_dma_read(mdev, dev->memmory_base2, targetBlock);
    pciedev_dma_release(mdev);
    
    return targetBlock;
}

int pciedev_dma_read(pciedev_dev* dev, u_int devOffset, u_int dataSize, void* userBuffer)
{
    int retVal = 0;
    u_int dmaSize   = PCIEDEV_DMA_SYZE * DIV_ROUND_UP(dataSize, PCIEDEV_DMA_SYZE);
    u_int dataReq   = 0;
    u_int dataRead  = 0;
    pciedev_buffer* prevBlock = 0;
    pciedev_buffer* nextBlock = 0;
    struct module_dev* mdev  = (struct module_dev*)(dev->dev_str);
    
    PDEBUG("pciedev_dma_read(devOffset=0x%lx, dataSize=0x%lx)\n",  devOffset, dataSize); 
    
    for (; !retVal && (dataRead < dmaSize); )
    {
        if (dataReq < dmaSize)
        {
            nextBlock = pciedev_start_dma(dev, devOffset + dataReq, dmaSize - dataReq);
            if (IS_ERR(nextBlock))
            {
                return PTR_ERR(nextBlock);
            }
            dataReq += nextBlock->dma_size;
        }
        
        if (!retVal && prevBlock)
        {
            retVal = pciedev_wait_dma_read(mdev, prevBlock);
            if (!retVal)
            {
                if (copy_to_user(userBuffer + dataRead, (void*) prevBlock->kaddr, min(prevBlock->dma_size, dataSize - dataRead)))
                {
                    retVal = -EFAULT;
                    break;
                }
                dataRead += prevBlock->dma_size;
                pciedev_buffer_set_free(mdev, prevBlock);
            }
        }
        
        prevBlock = nextBlock;
    }
    
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
    ulong           value;
    u_int           tmp_dma_size;
    u_int           tmp_dma_trns_size;
    u_int           tmp_dma_offset;
    u32             tmp_data_32;
    void*           pWriteBuf           = 0;
    void*           dma_reg_address;
    int             tmp_order           = 0;
    unsigned long   length              = 0;
    dma_addr_t      pTmpDmaHandle;
    u32             dma_sys_addr ;
    int             tmp_source_address  = 0;
    u_int           tmp_dma_control_reg = 0;
    u_int           tmp_dma_len_reg     = 0;
    u_int           tmp_dma_src_reg     = 0;
    u_int           tmp_dma_dest_reg    = 0;
    u_int           tmp_dma_cmd         = 0;
    long            timeDMAwait;

    int size_time;
    int io_dma_size;
    device_ioctrl_time  time_data;
    device_ioctrl_dma   dma_data;
    
    module_dev       *module_dev_pp;
    pciedev_dev       *dev  = filp->private_data;
    module_dev_pp = (module_dev*)(dev->dev_str);
    
    cmd         = *cmd_p;
    arg         = *arg_p;
    size_time   = sizeof(device_ioctrl_time);
    io_dma_size = sizeof(device_ioctrl_dma);
    minor       = dev->dev_minor;
    d_num       = dev->dev_num;	
    cur_proc    = current->group_leader->pid;
    pdev        = (dev->pciedev_pci_dev);

    PDEBUG("pciedev_ioctl_dma(filp=0x%lx, nr=%d )", filp, _IOC_NR(cmd));
    
    if(!dev->dev_sts){
        printk(KERN_ALERT "PCIEDEV_IOCTRL: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
        
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
            
        case PCIEDEV_READ_DMA:
            retval = 0;
            if (copy_from_user(&dma_data, (device_ioctrl_dma*)arg, (size_t)io_dma_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
             if(!dev->memmory_base2){
                printk("SIS8300_IOCTL_DMA: NO MEMORY\n");
                retval = -ENOMEM;
                return retval;
            }
            dma_reg_address = dev->memmory_base2;

            tmp_dma_control_reg = (dma_data.dma_reserved1 >> 16) & 0xFFFF;
            tmp_dma_len_reg     = dma_data.dma_reserved1 & 0xFFFF;
            tmp_dma_src_reg     = (dma_data.dma_reserved2 >> 16) & 0xFFFF;
            tmp_dma_dest_reg    = dma_data.dma_reserved2 & 0xFFFF;
            tmp_dma_cmd         = dma_data.dma_cmd;
            tmp_dma_size        = dma_data.dma_size;
            tmp_dma_offset      = dma_data.dma_offset;

/*
            printk (KERN_ALERT "PCIEDEV_READ_DMA: tmp_dma_control_reg %X, tmp_dma_len_reg %X\n",
                   tmp_dma_control_reg, tmp_dma_len_reg);
            printk (KERN_ALERT "PCIEDEV_READ_DMA: tmp_dma_src_reg %X, tmp_dma_dest_reg %X\n",
                   tmp_dma_src_reg, tmp_dma_dest_reg);
            printk (KERN_ALERT "PCIEDEV_READ_DMA: tmp_dma_cmd %X, tmp_dma_size %X\n",
                   tmp_dma_cmd, tmp_dma_size);
*/
            
            module_dev_pp->dev_dma_size     = tmp_dma_size;
             if(tmp_dma_size <= 0){
                 mutex_unlock(&dev->dev_mut);
                 return -EFAULT;
            }
            tmp_dma_trns_size    = tmp_dma_size;
            if((tmp_dma_size%PCIEDEV_DMA_SYZE)){
                tmp_dma_trns_size    = tmp_dma_size + (tmp_dma_size%PCIEDEV_DMA_SYZE);
            }
            value    = HZ/1; /* value is given in jiffies*/
            length   = tmp_dma_size;
            tmp_order = get_order(tmp_dma_trns_size);
            module_dev_pp->dma_order = tmp_order;
            pWriteBuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_DMA, tmp_order);
            if (!pWriteBuf) {
                PDEBUG("pciedev_ioctl_dma(): Failed to allocate buffer of order %d", tmp_order);
                mutex_unlock(&dev->dev_mut);
                return -ENOMEM;
            }
            
            PDEBUG("pciedev_ioctl_dma(): Allocated buffer: 0x%lx of order %d", pWriteBuf, tmp_order);
            
            // TODO: Remove this
            // memset(pWriteBuf, 0xE9, tmp_dma_trns_size);
            
            pTmpDmaHandle      = pci_map_single(pdev, pWriteBuf, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);

            /* MAKE DMA TRANSFER*/
            tmp_source_address = tmp_dma_offset;
            dma_sys_addr       = (u32)(pTmpDmaHandle & 0xFFFFFFFF);
            iowrite32(tmp_source_address, ((void*)(dma_reg_address + DMA_BOARD_ADDRESS)));
            tmp_data_32         = dma_sys_addr;
            iowrite32(tmp_data_32, ((void*)(dma_reg_address + DMA_CPU_ADDRESS)));
            smp_wmb();
            //udelay(5);
            tmp_data_32       = ioread32(dma_reg_address + 0x0); // be safe all writes are done
            smp_rmb();
            tmp_data_32         = tmp_dma_trns_size;
            do_gettimeofday(&(module_dev_pp->dma_start_time));
            module_dev_pp->waitFlag = 0;
            
            // TODO: switch back
            iowrite32(tmp_data_32, ((void*)(dma_reg_address + DMA_SIZE_ADDRESS)));
            //iowrite32(0, ((void*)(dma_reg_address + DMA_SIZE_ADDRESS)));
            timeDMAwait = wait_event_interruptible_timeout( module_dev_pp->waitDMA, module_dev_pp->waitFlag != 0, value );
            do_gettimeofday(&(module_dev_pp->dma_stop_time));
             if(!module_dev_pp->waitFlag){
                printk (KERN_ALERT "SIS8300_READ_DMA:SLOT NUM %i NO INTERRUPT \n", dev->slot_num);
                module_dev_pp->waitFlag = 1;
                pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
                free_pages((ulong)pWriteBuf, (ulong)module_dev_pp->dma_order);
                mutex_unlock(&dev->dev_mut);
                return -EFAULT;
            }
            pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
             if (copy_to_user ((void *)arg, pWriteBuf, tmp_dma_size)) {
                retval = -EFAULT;
            }
            free_pages((ulong)pWriteBuf, (ulong)module_dev_pp->dma_order);
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
                
        case PCIEDEV_KRING_READ_DMA:
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

