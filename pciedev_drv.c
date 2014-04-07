#include <linux/module.h>
#include <linux/fs.h>	
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/mm.h>

#include "pciedev_fnc.h"
#include "pciedev_buffer.h"
#include "pciedev_dma.h"

MODULE_AUTHOR("Ludwig Petrosyan");
MODULE_DESCRIPTION("DESY AMC-PCIE board driver");
MODULE_VERSION("2.0.0");
MODULE_LICENSE("Dual BSD/GPL");

static unsigned long  kbuf_blk_sz_kb = 4096;
static unsigned short kbuf_blk_num   = 2;

module_param(kbuf_blk_sz_kb, ulong, S_IRUGO);
module_param(kbuf_blk_num, ushort, S_IRUGO);


pciedev_cdev     *pciedev_cdev_m = 0;
//pciedev_dev       *pciedev_dev_m   = 0;
module_dev       *module_dev_p[PCIEDEV_NR_DEVS];
module_dev       *module_dev_pp;

static int     pciedev_open( struct inode *inode, struct file *filp );
static int     pciedev_release(struct inode *inode, struct file *filp);
static ssize_t pciedev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t pciedev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static long    pciedev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int     pciedev_mmap(struct file *filp, struct vm_area_struct *vma);

struct file_operations pciedev_fops = {
    .owner              =  THIS_MODULE,
    .read               =  pciedev_read,
    .write              =  pciedev_write,
    .unlocked_ioctl     =  pciedev_ioctl,
    .open               =  pciedev_open,
    .release            =  pciedev_release,
    .mmap               =  pciedev_mmap              
};

/**
 * mmap operations struct.
 */
static struct vm_operations_struct pciedev_vmops = {
    // TODO: nothing is needed here?
};

static struct pci_device_id pciedev_ids[]  = {
    { PCIEDEV_VENDOR_ID, PCIEDEV_DEVICE_ID, PCIEDEV_SUBVENDOR_ID, PCIEDEV_SUBDEVICE_ID, 0, 0, 0},
    // TODO: this was added so that driver would bind to the test-firmware. Should probably remove it or ask for complete list of supported device IDs
    { PCIEDEV_VENDOR_ID, 0x0037,            PCIEDEV_SUBVENDOR_ID, PCIEDEV_SUBDEVICE_ID, 0, 0, 0},     
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pciedev_ids);

/*
 * The top-half interrupt handler.
 */
#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t sis8300_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t pciedev_interrupt(int irq, void *dev_id)
#endif
{
    uint32_t intreg = 0;
    
    struct pciedev_dev *pdev = (pciedev_dev*)dev_id;
    struct module_dev *dev   = (module_dev *)(pdev->dev_str);
    
    PDEBUG("pciedev_interrupt(DMA_IRQ)\n");
    
    if (dev->dma_buffer)
    {
        PDEBUG("pciedev_interrupt(dma_offset=0x%lx, dma_size=0x%lx, drv_offset=0x%lx).\n", dev->dma_buffer->dma_offset, dev->dma_buffer->offset); 
        
        dev->dma_buffer->dma_done = 1;
        dev->buffer_nrRead += 1;
    }
    wake_up_interruptible(&(dev->buffer_waitQueue));
    
    dev->waitFlag = 1;
    wake_up_interruptible(&(dev->waitDMA));
    PDEBUG("pciedev_interrupt(DMA_IRQ): exit\n");
    
    return IRQ_HANDLED;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
    static int pciedev_probe(struct pci_dev *dev, const struct pci_device_id *id)
#else 
    static int __devinit pciedev_probe(struct pci_dev *dev, const struct pci_device_id *id)
#endif
{  
    int result               = 0;
    int tmp_brd_num = -1;
    
    printk(KERN_ALERT "PCIEDEV_PROBE CALLED \n");
    result = pciedev_probe_exp(dev, id, &pciedev_fops, &pciedev_cdev_m, DEVNAME, &tmp_brd_num);
    printk(KERN_ALERT "PCIEDEV_PROBE_EXP CALLED  FOR BOARD %i\n", tmp_brd_num);
    /*if board has created we will create our structure and pass it to pcedev_dev*/
    if(!result){
        printk(KERN_ALERT "PCIEDEV_PROBE_EXP CREATING CURRENT STRUCTURE FOR BOARD %i\n", tmp_brd_num);
        module_dev_p[tmp_brd_num] = pciedev_create_drvdata(tmp_brd_num, kbuf_blk_num, kbuf_blk_sz_kb * 1024, pciedev_cdev_m->pciedev_dev_m[tmp_brd_num]);
        printk(KERN_ALERT "PCIEDEV_PROBE CALLED; CURRENT STRUCTURE CREATED \n");
        
        pciedev_set_drvdata(pciedev_cdev_m->pciedev_dev_m[tmp_brd_num], module_dev_p[tmp_brd_num]);
        pciedev_setup_interrupt(pciedev_interrupt, pciedev_cdev_m->pciedev_dev_m[tmp_brd_num], DEVNAME); 
        
        // TODO: move this into upciedev
        INIT_WORK(&module_dev_p[tmp_brd_num]->dma_work, pciedev_dma_req_handler);
    }
    return result;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
    static void pciedev_remove(struct pci_dev *dev)
#else 
   static void __devexit pciedev_remove(struct pci_dev *dev)
#endif
{
     int result               = 0;
     int tmp_slot_num = -1;
     int tmp_brd_num = -1;
     printk(KERN_ALERT "REMOVE CALLED\n");
     tmp_brd_num =pciedev_get_brdnum(dev);
     printk(KERN_ALERT "REMOVE CALLED FOR BOARD %i\n", tmp_brd_num);
     /* clean up any allocated resources and stuff here */
     pciedev_release_drvdata(module_dev_p[tmp_brd_num]);
     
     /*now we can call pciedev_remove_exp to clean all standard allocated resources
      will clean all interrupts if it seted 
      */
     result = pciedev_remove_exp(dev,  &pciedev_cdev_m, DEVNAME, &tmp_slot_num);
     printk(KERN_ALERT "PCIEDEV_REMOVE_EXP CALLED  FOR SLOT %i\n", tmp_slot_num);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
    static struct pci_driver pci_pciedev_driver = {
    .name       = DEVNAME,
    .id_table   = pciedev_ids,
    .probe      = pciedev_probe,
    .remove    = pciedev_remove,
};
#else 
   static struct pci_driver pci_pciedev_driver = {
    .name       = DEVNAME,
    .id_table   = pciedev_ids,
    .probe      = pciedev_probe,
    .remove     = __devexit_p(pciedev_remove),
};
#endif


static int pciedev_open( struct inode *inode, struct file *filp )
{
    int    result = 0;
    result = pciedev_open_exp( inode, filp );
    return result;
}

static int pciedev_release(struct inode *inode, struct file *filp)
{
    int result            = 0;
    result = pciedev_release_exp(inode, filp);
    return result;
} 

static ssize_t pciedev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t    retval         = 0;
    retval  = pciedev_read_exp(filp, buf, count, f_pos);
    return retval;
}

static ssize_t pciedev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t         retval = 0;
    retval = pciedev_write_exp(filp, buf, count, f_pos);
    return retval;
}

static long  pciedev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long result = 0;
    
    //if (_IOC_TYPE(cmd) != PCIEDOOCS_IOC) return -ENOTTY; /*For future check if cmd is right*/
    if (_IOC_TYPE(cmd) == PCIEDOOCS_IOC){
        if (_IOC_NR(cmd) <= PCIEDOOCS_IOC_MAXNR && _IOC_NR(cmd) >= PCIEDOOCS_IOC_MINNR) {
            result = pciedev_ioctl_exp(filp, &cmd, &arg, pciedev_cdev_m);
        }else{
            result = pciedev_ioctl_dma(filp, &cmd, &arg, pciedev_cdev_m);
        }
    }else{
        return -ENOTTY;
    }
    return result;
}

int pciedev_mmap(struct file *filp, struct vm_area_struct *vma) 
{
    pciedev_dev   *dev  = filp->private_data;
    module_dev    *mdev = (module_dev*)(dev->dev_str);
    unsigned long mmap_pfn;
    unsigned long size;
    unsigned long offset;
    pciedev_block *block;
    int           status;
    
    if (vma->vm_end < vma->vm_start) {
        return -EINVAL;
    }
    
    if ((vma->vm_flags & VM_EXEC) || !(vma->vm_flags & VM_READ)) {
        return -EACCES;
    }
    
    size   = vma->vm_end - vma->vm_start;
    offset = vma->vm_pgoff * PAGE_SIZE;
    
    /* Map kernel buffer block */
    PDEBUG("SPIN-LOCK\n");
    spin_lock(&mdev->dma_bufferList_lock);
    
    /* Find target block */
    block = 0;
    struct list_head *ptr; 
    list_for_each(ptr, &mdev->dma_bufferList) 
    {
        block = list_entry(ptr, struct pciedev_block, list);
        if ((block->offset == offset) && (block->size == size))
        {
            break;
        }
        block = 0;
    }
    spin_unlock(&mdev->dma_bufferList_lock);

    if(!block) 
    {
        return -ENOMEM;
    }
    
    PDEBUG("pciedev_mmap(offset=0x%lx, size=0x%lx): Found!\n", offset, size); 
    
    /* Map kernel memory into address space of a userspace process. */
    mmap_pfn = virt_to_phys((void *)block->kaddr) >> PAGE_SHIFT;
    status = remap_pfn_range(vma, vma->vm_start, mmap_pfn, size, vma->vm_page_prot);
    if (status) {
        printk(KERN_ERR "PCIEDEV: mmap can't remap memory to userspace\n");
    }
    
    /* VM_IO | VM_DONTEXPAND | VM_DONTDUMP are set by remap_pfn_range() so
     * vma->vm_flags |= VM_RESERVED is not needed anymore. */
    vma->vm_ops = &pciedev_vmops;
    vma->vm_private_data = block;
    
    return status;
}


static void __exit pciedev_cleanup_module(void)
{
    printk(KERN_NOTICE "PCIEDEV_CLEANUP_MODULE CALLED\n");
    pci_unregister_driver(&pci_pciedev_driver);
    printk(KERN_NOTICE "PCIEDEV_CLEANUP_MODULE: PCI DRIVER UNREGISTERED\n");
}

static int __init pciedev_init_module(void)
{
    int   result  = 0;
    
    printk(KERN_ALERT "AFTER_INIT:REGISTERING PCI DRIVER\n");
    result = pci_register_driver(&pci_pciedev_driver);
    printk(KERN_ALERT "AFTER_INIT:REGISTERING PCI DRIVER RESUALT %d\n", result);
    return 0; /* succeed */
}

module_init(pciedev_init_module);
module_exit(pciedev_cleanup_module);


