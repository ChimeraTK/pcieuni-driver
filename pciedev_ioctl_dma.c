#include <linux/types.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#include "pciedev_dma.h"
#include "pciedev_fnc.h"
#include "pciedev_io.h"
#include "pciedev_ufn.h"

void pciedev_unpack_dma_desc(void* reg_address, device_ioctrl_dma* data, dma_desc* dma)
{
    dma->reg_address = reg_address;
    dma->control_reg = (data->dma_reserved1 >> 16) & 0xFFFF;
    dma->len_reg     = data->dma_reserved1 & 0xFFFF;
    dma->src_reg     = (data->dma_reserved2 >> 16) & 0xFFFF;
    dma->dest_reg    = data->dma_reserved2 & 0xFFFF;
    dma->cmd         = data->dma_cmd;
    dma->size        = data->dma_size;
    dma->offset      = data->dma_offset;
    dma->trans_size  = DIV_ROUND_UP(data->dma_size, PCIEDEV_DMA_SYZE);
}

pciedev_block* pciedev_bufring_get_writable(module_dev* dev, dma_desc* dma)
{
    pciedev_block* dma_buffer;
    ulong  timeout = HZ/1;

    PDEBUG("pciedev_bufring_get_writable(dma_offset=0x%lx, dma_size=ox%lx)", dma->offset, dma->trans_size);
    
    dma_buffer = 0;
    PDEBUG("SPIN-LOCK\n");
    spin_lock(&dev->dma_bufferList_lock);
    
    if (!list_empty(&dev->dma_bufferList)) 
    {
        dma_buffer = list_first_entry(&dev->dma_bufferList, struct pciedev_block, list);
    }

    while (dma_buffer && !dma_buffer->dma_free)
    { 
        if ((dev->dma_buffer == dma_buffer) && !list_is_singular(&dev->dma_bufferList))
        {
            // this buffer is being used by latest DMA... just go to next one
            list_rotate_left(&dev->dma_bufferList);
        }
        else
        {
            spin_unlock(&dev->dma_bufferList_lock);
            PDEBUG("SPIN-UNLOCKED\n");
            
            // wait until buffer is available
            //int code = wait_event_interruptible_timeout(dev->buffer_waitQueue, dev->buffer_waitFlag != 0, timeout);
            int code = wait_event_interruptible_timeout(dev->buffer_waitQueue, (dma_buffer->dma_free || (dev->dma_buffer == dma_buffer)) , timeout);
            if (code == 0)
            {
                printk(KERN_ALERT "PCIEDEV: Failed to get free memory buffer (TIMEOUT)\n");
                return 0;
            }
            else if (code < 0 )
            {
                printk(KERN_ALERT "PCIEDEV: Failed to get free memory buffer (errno=%d)\n", code);
                return 0;
            }
            
            PDEBUG("SPIN-LOCK\n");
            spin_lock(&dev->dma_bufferList_lock);
        }
        
        dma_buffer = list_first_entry(&dev->dma_bufferList, struct pciedev_block, list);
    }

    if (dma_buffer)
    {
        dma_buffer->dma_free   = 0;
        dma_buffer->dma_done   = 0;
        dma_buffer->dma_offset = dma->offset;
        dma_buffer->dma_size   = dma->trans_size;
        
        dev->buffer_waitFlag = 0;
    }
    
    spin_unlock(&dev->dma_bufferList_lock);
    PDEBUG("SPIN-UNLOCKED\n");
    
    if (dma_buffer)
    {   
        PDEBUG("pciedev_bufring_get_writable: Got available buffer (drv_offset=0x%lx, size=0x%lx)", dma_buffer->offset, dma_buffer->size);
    }
    
    return dma_buffer;
}

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

int pciedev_dma_release(module_dev* mdev)
{
    up(&mdev->dma_sem);
    return 0;
}

void pciedev_start_dma_read(module_dev* mdev, dma_desc* dma, pciedev_block* block)
{
    u32 tmp_data_32;

    PDEBUG("pciedev_start_dma_read(dma_offset=0x%lx, dma_size=0x%lx, drv_offset=0x%lx)\n", block->dma_offset, block->dma_size, block->offset); 
    //print_hex_dump_bytes("Buffer contents before DMA: ", DUMP_PREFIX_NONE, block->kaddr, 64);
    
    dma_sync_single_for_device(&mdev->parent_dev->pciedev_pci_dev->dev, block->dma_handle, (size_t)block->size, DMA_FROM_DEVICE);
    
    iowrite32(block->dma_offset,                     (void*)(dma->reg_address + DMA_BOARD_ADDRESS));
    iowrite32((u32)(block->dma_handle & 0xFFFFFFFF), (void*)(dma->reg_address + DMA_CPU_ADDRESS  ));

    smp_wmb();
    udelay(5);
    tmp_data_32       = ioread32(dma->reg_address + 0x0); // be safe all writes are done
    smp_rmb();

    do_gettimeofday(&(mdev->dma_start_time));
    
    mdev->waitFlag = 0;
    
    PDEBUG("SPIN-LOCK\n");
    spin_lock(&mdev->dma_bufferList_lock);
    mdev->dma_buffer = block; 
    mdev->buffer_waitFlag = 1;
    wake_up_interruptible(&(mdev->buffer_waitQueue));
    spin_unlock(&mdev->dma_bufferList_lock);
    PDEBUG("SPIN-UNLOCKED\n");
    
    //start DMA
    iowrite32(block->dma_size,                       (void*)(dma->reg_address + DMA_SIZE_ADDRESS ));
}

pciedev_block* pciedev_bufring_find_buffer(module_dev* dev, dma_desc* dma)
{
    pciedev_block* block;
    struct list_head *ptr;    

    PDEBUG("SPIN-LOCK\n");
    spin_lock(&dev->dma_bufferList_lock);
    list_for_each(ptr, &dev->dma_bufferList) 
    {
        block = list_entry(ptr, struct pciedev_block, list);
        if ((block->dma_offset == dma->offset) && (block->dma_size == dma->trans_size))
        {
            spin_unlock(&dev->dma_bufferList_lock);
            PDEBUG("SPIN-UNLOCKED\n");
            
            return block;
        }
    }
    spin_unlock(&dev->dma_bufferList_lock);
    PDEBUG("SPIN-UNLOCKED\n");
    return 0;
}

pciedev_block* pciedev_bufring_find_driver_buffer(module_dev* dev, unsigned long offset)
{
    pciedev_block* block;
    struct list_head *ptr;    
    
    PDEBUG("SPIN-LOCK\n");
    spin_lock(&dev->dma_bufferList_lock);
    
    list_for_each(ptr, &dev->dma_bufferList) 
    {
        block = list_entry(ptr, struct pciedev_block, list);
        if (block->offset == offset)
        {
            spin_unlock(&dev->dma_bufferList_lock);
            PDEBUG("SPIN-UNLOCKED\n");
            PDEBUG("pciedev_bufring_find_driver_buffer(offset=0x%lx): Found!\n", offset); 
            
            return block;
        }
    }
    spin_unlock(&dev->dma_bufferList_lock);
    PDEBUG("SPIN-UNLOCKED\n");
    
    
    printk(KERN_ALERT "PCIEDEV: Requested DMA buffer not found (offset=0x%lx)!\n", offset); 
    return 0;
}


// TODO: What if relese comes before read done!
void pciedev_bufring_release_buffer(module_dev* dev, pciedev_block* block)
{
    PDEBUG("pciedev_bufring_release_buffer(offset=0x%lx, size=0x%lx)\n", block->dma_offset, block->dma_size);

    PDEBUG("SPIN-LOCK\n");
    spin_lock(&dev->dma_bufferList_lock);
      
    block->dma_done = 0;
    block->dma_free = 1;
    dev->buffer_nrRead -= 1;
    
    if (list_first_entry(&dev->dma_bufferList, struct pciedev_block, list) == block)
    {
        dev->buffer_waitFlag = 1;
        wake_up_interruptible(&(dev->buffer_waitQueue));
    }
    
    spin_unlock(&dev->dma_bufferList_lock);
    PDEBUG("SPIN-UNLOCKED\n");
}

int pciedev_wait_dma_read(module_dev* dev, pciedev_block* block)
{
    ulong timeout = HZ/1;
    
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

int pciedev_req_kring_dma(pciedev_dev *dev, device_ioctrl_dma* dma_arg)
{
    dma_desc          desc;
    pciedev_block*    block;
    module_dev*       mdev;

    PDEBUG("pciedev_req_kring_dma(dma_offset=0x%lx, dma_size=0x%lx)\n", dma_arg->dma_offset, dma_arg->dma_size);
    
    if(!dev->memmory_base2) {
        return -ENOMEM;
    }
    
    mdev = (module_dev*)pciedev_get_drvdata(dev);
    if(!mdev) {
        return -EFAULT;
    }    
    
    pciedev_unpack_dma_desc(dev->memmory_base2, dma_arg, &desc);
    if(desc.size <= 0) {
        return -EFAULT;
    }    
    
    block = pciedev_bufring_get_writable(mdev, &desc);
    if(!block) {
        return -ENOMEM;
    }    
       
    if (pciedev_dma_reserve(mdev)) {
        return -EBUSY;
    }
    
    pciedev_start_dma_read(mdev, &desc, block);
    pciedev_dma_release(mdev);

    return 0;
}

int pciedev_wait_kring_dma(pciedev_dev *dev, device_ioctrl_dma* dma_arg, pciedev_block** pblock)
{
    dma_desc          desc;
    module_dev*       mdev;
    ulong  timeout = HZ/1;
    
    PDEBUG("pciedev_wait_kring_dma(dma_offset=0x%lx, dma_size=0x%lx)\n", dma_arg->dma_offset, dma_arg->dma_size);
    
    *pblock = 0;
    
    if(!dev->memmory_base2) {
        return -ENOMEM;
    }
    
    mdev = (module_dev*)pciedev_get_drvdata(dev);
    if(!mdev) {
        return -EFAULT;
    }    
    
    pciedev_unpack_dma_desc(dev->memmory_base2, dma_arg, &desc);
    if(desc.size <= 0) {
        return -EFAULT;
    }    
    
    while (!(*pblock))
    {
        int code = wait_event_interruptible_timeout(mdev->buffer_waitQueue, 0 != pciedev_bufring_find_buffer(mdev, &desc), timeout);
        if (code <= 0)
        {
            printk(KERN_ALERT "PCIEDEV: Requested DMA buffer not found (dma_offset=0x%lx, dma_size=0x%lx): TIMEOUT!\n", desc.offset, desc.trans_size);
            return -EFAULT;;
        }
        else if (code < 0 )
        {
            printk(KERN_ALERT "PCIEDEV: Requested DMA buffer not found (dma_offset=0x%lx, dma_size=0x%lx): errno=%d!\n", desc.offset, desc.trans_size, code);
            return -EFAULT;;
        }
        
        *pblock = pciedev_bufring_find_buffer(mdev, &desc);
    }
    if (!(*pblock))
    {
        return -EFAULT;
    }

    PDEBUG("pciedev_wait_kring_dma(dma_offset=0x%lx, dma_size=0x%lx): Target block found (drv_offset=0x%lx, size=0x%lx)\n", 
           dma_arg->dma_offset, dma_arg->dma_size, (*pblock)->offset, (*pblock)->size); 
    
    //print_hex_dump_bytes("Buffer contents after DMA: ", DUMP_PREFIX_NONE, (*pblock)->kaddr, 64);
    
    if (pciedev_wait_dma_read(mdev, *pblock))
    {
        // TODO: report proper error
        return -EFAULT;
    }
        //     // TODO: handle missing interrupt
    //     if (!module_dev_pp->waitFlag)
    //     {
        //         printk (KERN_ALERT "SIS8300_READ_DMA:SLOT NUM %i NO INTERRUPT \n", dev->slot_num);
        //         module_dev_pp->waitFlag = 1;
        //         return -EFAULT;
        //     }
    
    return 0;
}
        
long     pciedev_ioctl_dma(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, pciedev_cdev * pciedev_cdev_m)
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
                 return EFAULT;
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
            
            PDEBUG("pciedev_ioctl_dma(): Allocated buffer: 0x%lx of order %d", pWriteBuf, tmp_order);
            
            // TODO: Remove this
            memset(pWriteBuf, 0xE9, tmp_dma_trns_size);
            
            pTmpDmaHandle      = pci_map_single(pdev, pWriteBuf, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);

            /* MAKE DMA TRANSFER*/
            tmp_source_address = tmp_dma_offset;
            dma_sys_addr       = (u32)(pTmpDmaHandle & 0xFFFFFFFF);
            iowrite32(tmp_source_address, ((void*)(dma_reg_address + DMA_BOARD_ADDRESS)));
            tmp_data_32         = dma_sys_addr;
            iowrite32(tmp_data_32, ((void*)(dma_reg_address + DMA_CPU_ADDRESS)));
            smp_wmb();
            udelay(5);
            tmp_data_32       = ioread32(dma_reg_address + 0x0); // be safe all writes are done
            smp_rmb();
            tmp_data_32         = tmp_dma_trns_size;
            do_gettimeofday(&(module_dev_pp->dma_start_time));
            module_dev_pp->waitFlag = 0;
            iowrite32(tmp_data_32, ((void*)(dma_reg_address + DMA_SIZE_ADDRESS)));
            timeDMAwait = wait_event_interruptible_timeout( module_dev_pp->waitDMA, module_dev_pp->waitFlag != 0, value );
            do_gettimeofday(&(module_dev_pp->dma_stop_time));
             if(!module_dev_pp->waitFlag){
                printk (KERN_ALERT "SIS8300_READ_DMA:SLOT NUM %i NO INTERRUPT \n", dev->slot_num);
                module_dev_pp->waitFlag = 1;
                pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
                free_pages((ulong)pWriteBuf, (ulong)module_dev_pp->dma_order);
                mutex_unlock(&dev->dev_mut);
                return EFAULT;
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
                pciedev_block* block = list_first_entry(&module_dev_pp->dma_bufferList, struct pciedev_block, list);
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
                
        case PCIEDEV_KBUF_READ_DMA:
            
            if (copy_from_user(&dma_data, (void*)arg, io_dma_size)) {
                mutex_unlock(&dev->dev_mut);
                return -EFAULT;
            }
            
            retval = pciedev_req_kring_dma(dev, &dma_data);
            if (!retval)
            {
                pciedev_block* block;
                
                retval = pciedev_wait_kring_dma(dev, &dma_data, &block);
                if (block)
                {
                    if (copy_to_user((void *)arg, block->kaddr, dma_data.dma_size))
                    {
                        retval = -EFAULT;
                    }
                    pciedev_bufring_release_buffer(module_dev_pp, block);
                }
            }
            break;

        case PCIEDEV_REQUEST_READ_DMA:
            mutex_unlock(&dev->dev_mut);
            
            if (copy_from_user(&dma_data, (void*)arg, io_dma_size)) {
                return -EFAULT;
            }
            
            return pciedev_req_kring_dma(dev, &dma_data);
            break;

        case PCIEDEV_WAIT_READ_DMA:          
        {
            mutex_unlock(&dev->dev_mut);
            
            if (copy_from_user(&dma_data, (void*)arg, io_dma_size)) {
                return -EFAULT;
            }
            
            pciedev_block* block;
            
            retval = pciedev_wait_kring_dma(dev, &dma_data, &block);
            if (block)
            {
                if (copy_to_user((void *)arg, block->kaddr, dma_data.dma_size))
                {
                    retval = -EFAULT;
                }
                pciedev_bufring_release_buffer(module_dev_pp, block);
            }
            
            return retval;            
        }
        
        case PCIEDEV_WAIT_MMAP_KBUF:
        {
            mutex_unlock(&dev->dev_mut);
            
            if (copy_from_user(&dma_data, (void*)arg, io_dma_size)) {
                return -EFAULT;
            }
            
            pciedev_block* block;
            retval = pciedev_wait_kring_dma(dev, &dma_data, &block);
            if (block)
            {
                dma_data.dbuf_offset = block->offset;
                dma_data.dbuf_size   = block->size;
                
                if (copy_to_user((void*)arg, &dma_data, sizeof(device_ioctrl_dma)))
                {
                    retval = -EFAULT;
                }
            }
            
            return retval;
        }   
        case PCIEDEV_RELEASE_MMAP_KBUF:
        {
            if (copy_from_user(&dma_data, (device_ioctrl_dma*)arg, io_dma_size)) {
                mutex_unlock(&dev->dev_mut);
                return -EFAULT;
            }
            
            pciedev_block* block = pciedev_bufring_find_driver_buffer(module_dev_pp, dma_data.dbuf_offset);
            if (block)
            {
                pciedev_bufring_release_buffer(module_dev_pp, block);
            }
            else
            {
                retval = -EFAULT;
            }
            break;
        }   
        default:
            return -ENOTTY;
            break;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;
}

