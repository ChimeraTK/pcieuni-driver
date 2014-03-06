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

long     pciedev_ioctl_dma(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, pciedev_cdev * pciedev_cdev_m)
{
    unsigned int    cmd;
    unsigned long arg;
    pid_t                cur_proc = 0;
    int                    minor    = 0;
    int                    d_num    = 0;
    int                    retval   = 0;
    int                    err      = 0;
    struct pci_dev* pdev;
    ulong           value;
    u_int	    tmp_dma_size;
    u_int	    tmp_dma_trns_size;
    u_int	    tmp_dma_offset;
    u32            tmp_data_32;
    void*           pWriteBuf           = 0;
    void*          dma_reg_address;
    int             tmp_order           = 0;
    unsigned long   length              = 0;
    dma_addr_t      pTmpDmaHandle;
    u32             dma_sys_addr ;
    int               tmp_source_address  = 0;
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

    cmd              = *cmd_p;
    arg                = *arg_p;
    size_time      = sizeof (device_ioctrl_time);
    io_dma_size = sizeof(device_ioctrl_dma);
    minor           = dev->dev_minor;
    d_num         = dev->dev_num;	
    cur_proc     = current->group_leader->pid;
    pdev            = (dev->pciedev_pci_dev);
    
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
            
            module_dev_pp->dma_start_time.tv_sec+= (long)dev->slot_num;
            module_dev_pp->dma_stop_time.tv_sec  += (long)dev->slot_num;
            module_dev_pp->dma_start_time.tv_usec+= (long)dev->brd_num;
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
        default:
            return -ENOTTY;
            break;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;
}
