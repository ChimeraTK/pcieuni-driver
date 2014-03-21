#include <linux/types.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/delay.h>

#include "pciedev_fnc.h"
#include "pciedev_io.h"
#include "pciedev_ufn.h"
#include "pciedev_buffer.h"

struct dma_desc
{
    void*  reg_address;
    u_int  control_reg;
    u_int  len_reg;
    u_int  src_reg;
    u_int  dest_reg;
    u_int  cmd;
    u_int  size;
    u_int  trans_size;
    u_int  offset;
};
typedef struct dma_desc dma_desc;

void pciedev_unpack_dma_desc(void* reg_address, device_ioctrl_dma* data, dma_desc* dma)
{
    PDEBUG("pciedev_unpack_dma_desc()\n");
    
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

pciedev_block* pciedev_bufring_get_writable(module_dev* dev)
{
    pciedev_block* dma_buffer;
    ulong  timeout = HZ/1;

    PDEBUG("pciedev_bufring_get_writable()\n");
    
    spin_lock(&dev->dma_bufferList_lock);
    dma_buffer = list_entry(dev->dma_bufferList.next, struct pciedev_block, list);
    while (!dma_buffer->dma_free)
    {
        spin_unlock(&dev->dma_bufferList_lock);
        
        PDEBUG("pciedev_bufring_get_writable: wait for buffer available \n");
        if (0 >= wait_event_interruptible_timeout( dev->waitDMA, dev->waitFlag != 0, timeout))
        {
            return 0;
        }
        spin_lock(&dev->dma_bufferList_lock);
        dma_buffer = list_entry(dev->dma_bufferList.next, struct pciedev_block, list);
    }
    dma_buffer->dma_free   = 0;
    dma_buffer->dma_done   = 0;
    dma_buffer->dma_offset = 0;
    dma_buffer->dma_size   = 0;
    
    spin_unlock(&dev->dma_bufferList_lock);

    return dma_buffer;
}

int pciedev_dma_reserve(module_dev* dev)
{
    ulong timeout = HZ/1;
    
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
}

void pciedev_start_dma_read(module_dev* mdev, dma_desc* dma, pciedev_block* block)
{
    u32 tmp_data_32;

    // TODO: check if block is free ?
    block->dma_offset = dma->offset;
    block->dma_size   = dma->trans_size;
    
    iowrite32(block->dma_offset,                     (void*)(dma->reg_address + DMA_BOARD_ADDRESS));
    iowrite32((u32)(block->dma_handle & 0xFFFFFFFF), (void*)(dma->reg_address + DMA_CPU_ADDRESS  ));

    smp_wmb();
    udelay(5);
    tmp_data_32       = ioread32(dma->reg_address + 0x0); // be safe all writes are done
    smp_rmb();

    do_gettimeofday(&(mdev->dma_start_time));
    
    mdev->waitFlag = 0;
    iowrite32(block->dma_size,                       (void*)(dma->reg_address + DMA_SIZE_ADDRESS ));
}

pciedev_block* pciedev_bufring_find_buffer(module_dev* dev, dma_desc* dma)
{
    pciedev_block* block;
    
    PDEBUG("pciedev_bufring_find_buffer()");
    
    spin_lock(&dev->dma_bufferList_lock);
    struct list_head *ptr;
    list_for_each(ptr, &dev->dma_bufferList) 
    {
        block = list_entry(dev->dma_bufferList.next, struct pciedev_block, list);
        if ((block->dma_offset == dma->offset) && (block->dma_size == dma->trans_size))
        {
            PDEBUG("pciedev_bufring_find_buffer(): target buffer found \n"); 
            spin_unlock(&dev->dma_bufferList_lock);
            return block;
        }
    }
    spin_unlock(&dev->dma_bufferList_lock);
    
    PDEBUG("pciedev_bufring_find_buffer(): Cannot find target buffer. \n"); 
    return 0;
}


void pciedev_bufring_release_buffer(module_dev* dev, pciedev_block* block)
{
    PDEBUG("pciedev_bufring_release_buffer()\n");

    spin_lock(&dev->dma_bufferList_lock);
    block->dma_done = 0;
    block->dma_free = 1;
    spin_unlock(&dev->dma_bufferList_lock);

    PDEBUG("pciedev_bufring_release_buffer: Wake up waiters\n");
    wake_up_interruptible(&(dev->waitDMA));
}

int pciedev_wait_dma_read(module_dev* dev, pciedev_block* block)
{
    ulong timeout = HZ/1;
    PDEBUG("pciedev_wait_dma_read()");
    
    spin_lock(&dev->dma_bufferList_lock);
    while(!block->dma_done)
    {
        spin_unlock(&dev->dma_bufferList_lock);
        PDEBUG("pciedev_wait_dma_read: DMA not done yet. Waiting...\n");
            
        if (0 >= wait_event_interruptible_timeout( dev->waitDMA, dev->waitFlag != 0, timeout))
        {
            return -ERESTARTSYS; // TODO: check return code
        }

        spin_lock(&dev->dma_bufferList_lock);
    }
    spin_unlock(&dev->dma_bufferList_lock);
    
    do_gettimeofday(&(dev->dma_stop_time));
    return 0;
}

int pciedev_read_kbuf_dma(pciedev_dev *dev, unsigned long arg)
{
    //TODO: Rewrite
    
//     u_int           tmp_dma_control_reg = 0;
//     u_int           tmp_dma_len_reg     = 0;
//     u_int           tmp_dma_src_reg     = 0;
//     u_int           tmp_dma_dest_reg    = 0;
//     u_int           tmp_dma_cmd         = 0;
//     int             tmp_source_address  = 0;
//     u_int           tmp_dma_size;
//     u_int           tmp_dma_trns_size;
//     u_int           tmp_dma_offset;
//     device_ioctrl_dma   dma_data;
//     long            timeDMAwait;
//     ulong           value = HZ/1; /* value is given in jiffies*/
//     int             retval   = 0;
//     void*           dma_reg_address;
//     module_dev      *module_dev_pp;
//     u32             tmp_data_32;
//         
//     // read DMA parameters from arg
//     if (copy_from_user(&dma_data, (device_ioctrl_dma*)arg, sizeof(device_ioctrl_dma))) 
//     {
//         retval = -EFAULT;
//         return retval;
//     }
//     
//     if(!dev->memmory_base2){
//         printk("SIS8300_IOCTL_DMA: NO MEMORY\n");
//         retval = -ENOMEM;
//         return retval;
//     }
// 
//     dma_reg_address     = dev->memmory_base2;
//     tmp_dma_control_reg = (dma_data.dma_reserved1 >> 16) & 0xFFFF;
//     tmp_dma_len_reg     = dma_data.dma_reserved1 & 0xFFFF;
//     tmp_dma_src_reg     = (dma_data.dma_reserved2 >> 16) & 0xFFFF;
//     tmp_dma_dest_reg    = dma_data.dma_reserved2 & 0xFFFF;
//     tmp_dma_cmd         = dma_data.dma_cmd;
//     tmp_dma_size        = dma_data.dma_size;
//     tmp_dma_offset      = dma_data.dma_offset;
//     
//     module_dev_pp                = (module_dev*)(dev->dev_str);    
//     module_dev_pp->dev_dma_size  = tmp_dma_size;
//     
//     if(tmp_dma_size <= 0)
//     {
//         return EFAULT;
//     }
//     
//     // TODO: check this!!!
//     tmp_dma_trns_size    = tmp_dma_size;
//     if((tmp_dma_size%PCIEDEV_DMA_SYZE))
//     {
//         tmp_dma_trns_size    = tmp_dma_size + (tmp_dma_size%PCIEDEV_DMA_SYZE);
//     }
//     
//     pciedev_block* dma_buffer;
//     dma_buffer = list_entry(dev->dma_bufferList.next, struct pciedev_block, list);
//     
//     /* MAKE DMA TRANSFER*/
//     tmp_source_address = tmp_dma_offset;
//     u32 dma_sys_addr       = (u32)(dma_buffer->dma_handle & 0xFFFFFFFF);
//     iowrite32(tmp_source_address, ((void*)(dma_reg_address + DMA_BOARD_ADDRESS)));
//     tmp_data_32         = dma_sys_addr;
//     iowrite32(tmp_data_32, ((void*)(dma_reg_address + DMA_CPU_ADDRESS)));
//     smp_wmb();
//     udelay(5);
//     tmp_data_32       = ioread32(dma_reg_address + 0x0); // be safe all writes are done
//     smp_rmb();
//     tmp_data_32         = tmp_dma_trns_size;
//     do_gettimeofday(&(module_dev_pp->dma_start_time));
//     module_dev_pp->waitFlag = 0;
//     iowrite32(tmp_data_32, ((void*)(dma_reg_address + DMA_SIZE_ADDRESS)));
//     timeDMAwait = wait_event_interruptible_timeout( module_dev_pp->waitDMA, module_dev_pp->waitFlag != 0, value);
//     do_gettimeofday(&(module_dev_pp->dma_stop_time));
// 
//     if (!module_dev_pp->waitFlag)
//     {
//         printk (KERN_ALERT "SIS8300_READ_DMA:SLOT NUM %i NO INTERRUPT \n", dev->slot_num);
//         module_dev_pp->waitFlag = 1;
//         return EFAULT;
//     }
// 
//     if (copy_to_user ((void *)arg, dma_buffer->kaddr, tmp_dma_size)) 
//     {
//         retval = -EFAULT;
//     }
//     
    return -EFAULT;
}

int pciedev_req_kring_dma(pciedev_dev *dev, unsigned long arg)
{
    device_ioctrl_dma dma_arg;
    dma_desc          desc;
    pciedev_block*    block;
    module_dev*       mdev;

    PDEBUG("pciedev_req_kring_dma()\n");

    if(!dev->memmory_base2) {
        return -ENOMEM;
    }
    
    mdev = (module_dev*)pciedev_get_drvdata(dev);
    if(!mdev) {
        return -EFAULT;
    }    
    
    // read DMA request parameters from arg
    if (copy_from_user(&dma_arg, (device_ioctrl_dma*)arg, sizeof(device_ioctrl_dma))) {
        return -EFAULT;
    }
    pciedev_unpack_dma_desc(dev->memmory_base2, &dma_arg, &desc);
    if(desc.size <= 0) {
        return -EFAULT;
    }    
    
    block = pciedev_bufring_get_writable(mdev);
    if(!block) {
        return -EFAULT;
    }    
       
    if (pciedev_dma_reserve(mdev)) {
        return -EBUSY;
    }
    
    pciedev_start_dma_read(mdev, &desc, block);
    pciedev_dma_release(mdev);

    return 0;
}

int pciedev_get_kring_dma(pciedev_dev *dev, unsigned long arg)
{
    device_ioctrl_dma dma_arg;
    dma_desc          desc;
    pciedev_block*    block;
    module_dev*       mdev;
    
    PDEBUG("pciedev_get_kring_dma()\n");
    
    if(!dev->memmory_base2) {
        return -ENOMEM;
    }
    
    mdev = (module_dev*)pciedev_get_drvdata(dev);
    if(!mdev) {
        return -EFAULT;
    }    
    
    // read DMA request parameters from arg
    if (copy_from_user(&dma_arg, (device_ioctrl_dma*)arg, sizeof(device_ioctrl_dma))) {
        return -EFAULT;
    }
    pciedev_unpack_dma_desc(dev->memmory_base2, &dma_arg, &desc);
    if(desc.size <= 0) {
        return -EFAULT;
    }    
    
    block = pciedev_bufring_find_buffer(mdev, &desc);
    if (!block)
    {
        return -EFAULT;
    }
    
    if (pciedev_wait_dma_read(mdev, block))
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
    
    PDEBUG("pciedev_get_kring_dma: Copy to user\n");
    if (copy_to_user ((void *)arg, block->kaddr, desc.size)) 
    {
        return -EFAULT;
    }
    
    pciedev_bufring_release_buffer(mdev, block);
    
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
            
        case PCIEDEV_READ_KBUF_DMA:
            retval = pciedev_read_kbuf_dma(dev, arg);
            break;

        case PCIEDEV_READ_KRING_DMA:
            retval = pciedev_req_kring_dma(dev, arg);
            break;

        case PCIEDEV_GET_KRING_DMA:
            retval = pciedev_get_kring_dma(dev, arg);
            break;
            
        default:
            return -ENOTTY;
            break;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;
}

