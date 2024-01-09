/**
 *  @file   pcieuni_ioctl_dma.c
 *  @brief  Implementation of DMA related IOCTL handlers
 */

#include "pcieuni_fnc.h"
#include <gpcieuni/pcieuni_buffer.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/types.h>

#ifdef PCIEUNI_DEBUG
static atomic_t dma_request_counter = ATOMIC_INIT(0);
#endif

/**
 * @brief Initiates DMA read from device
 *
 * This function will initiate DMA read from device into the target buffer. The DMA transfer size and offset are taken
 * from the target buffer structure. The target buffer is expected to be already synched for device access.
 * In case the device is busy this fuction will block. Once the DMA transfer is initiated the function will return immediately.
 * @note This function may block.
 *
 * @param dev          Traget device structure
 * @param targetBuffer Target buffer
 *
 * @return   0       Success
 * @retval   -EBUSY  Cannot initiate DMA because target device is busy
 * @retval   -EINTR  Operation was interupted
 * @retval   -EIO    Failed to write to device registers
 */
int pcieuni_start_dma_read(pcieuni_dev* dev, pcieuni_buffer* targetBuffer) {
  struct module_dev* mdev = pcieuni_get_mdev(dev);
  int retVal = 0;

#ifdef PCIEUNI_DEBUG
  atomic_inc(&dma_request_counter);

  PDEBUG(dev->name, "pcieuni_start_dma_read(offset=0x%lx, maxSize=0x%lx) number %i\n", targetBuffer->dma_offset,
      targetBuffer->dma_size, atomic_read(&dma_request_counter));
#endif /*PCIEUNI_DEBUG*/

  // reserve device registers IO
  retVal = pcieuni_dma_reserve(mdev, targetBuffer);
  if(retVal) return retVal;

  // write DMA source address to device register
  retVal = pcieuni_register_write32(dev, dev->memmory_base2, DMA_BOARD_ADDRESS, targetBuffer->dma_offset, false);
  if(retVal) goto cleanup_releaseDevice;

  // write DMA destination address to device register
  retVal = pcieuni_register_write32(
      dev, dev->memmory_base2, DMA_CPU_ADDRESS, (u32)(targetBuffer->dma_handle & 0xFFFFFFFF), true);
  if(retVal) goto cleanup_releaseDevice;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
  do_gettimeofday(&(mdev->dma_start_time));
#else
  ktime_get_real_ts64(&(mdev->dma_start_time));
#endif

  // Setup env for irq handler
  mdev->waitFlag = 0;
  mdev->dma_buffer = targetBuffer;

  // write DMA size and start DMA
  retVal = pcieuni_register_write32(dev, dev->memmory_base2, DMA_SIZE_ADDRESS, targetBuffer->dma_size, false);
  if(retVal) goto cleanup_releaseDevice;

  PDEBUG(dev->name, "pcieuni_start_dma_read(): DMA started, offset=0x%lx, size=0x%lx \n", targetBuffer->dma_offset,
      targetBuffer->dma_size);

cleanup_releaseDevice:
  if(retVal) {
    // release device registers IO
    pcieuni_dma_release(mdev);
  }

  return retVal;
}

/**
 * @brief Waits until DMA read to target buffer is finished
 * @note This function may block.
 *
 * @param mdev   Target device
 * @param buffer Target driver buffer
 *
 * @retval 0            Success
 * @retval -EIO         Timed out while waiting for end of DMA IRQ
 * @retval -EINTR       Interrupted while waiting for end of DMA IRQ
 */
int pcieuni_wait_dma_read(module_dev* mdev, pcieuni_buffer* buffer) {
  int code;
  ulong timeout = HZ / 1; // Timeout in 1 second

  PDEBUG(
      mdev->parent_dev->name, "pcieuni_wait_dma_read(offset=0x%lx, size=0x%lx)", buffer->dma_offset, buffer->dma_size);
  while(test_bit(BUFFER_STATE_WAITING, &buffer->state)) {
    // DMA not finished yet - wait for IRQ handler
    PDEBUG(mdev->parent_dev->name, "pcieuni_wait_dma_read(offset=0x%lx, size=0x%lx): Waiting... \n", buffer->dma_offset,
        buffer->dma_size);

    code = wait_event_timeout(mdev->waitDMA, !test_bit(BUFFER_STATE_WAITING, &buffer->state), timeout);
    if(code == 0) {
      printk(KERN_ERR "pcieuni(%s): error waiting for DMA to buffer (offset=0x%lx, size=0x%lx): TIMEOUT!\n",
          mdev->parent_dev->name, buffer->dma_offset, buffer->dma_size);

      // assuming we missed the interrupt
      pcieuni_dma_release(mdev);
      return -EIO;
    }
    else if(code < 0) {
      printk(KERN_ERR "pcieuni(%s): error waiting for DMA to buffer (offset=0x%lx, size=0x%lx): errno=%d!\n",
          mdev->parent_dev->name, buffer->dma_offset, buffer->dma_size, code);

      // assuming we missed the interrupt
      pcieuni_dma_release(mdev);
      return -EINTR;
    }
  }

  PDEBUG(mdev->parent_dev->name, "pcieuni_wait_dma_read(offset=0x%lx, size=0x%lx): Done!", buffer->dma_offset,
      buffer->dma_size);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
  do_gettimeofday(&(mdev->dma_stop_time));
#else
  ktime_get_real_ts64(&(mdev->dma_stop_time));
#endif
  return 0;
}

/**
 * @brief Reads from board memory via DMA using driver allocated buffers
 *
 * @param dev         Target device
 * @param devOffset   DMA offset to read from
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
int pcieuni_dma_read(pcieuni_dev* dev, unsigned long devOffset, unsigned long dataSize, void* userBuffer) {
  int retVal = 0;
  unsigned long dmaSize =
      PCIEUNI_DMA_SYZE * DIV_ROUND_UP(dataSize, PCIEUNI_DMA_SYZE); // round up total read-size to page boundary
  unsigned long dataReq = 0;                                       // Total size of data that was requested from device
  unsigned long dataRead = 0;                                      // Total size of data read from device
  pcieuni_buffer* prevBuffer = 0;                                  // buffer used for read in previous loop
  pcieuni_buffer* nextBuffer = 0;                                  // buffer to read to in this loop
  struct module_dev* mdev = pcieuni_get_mdev(dev);

  PDEBUG(dev->name, "pcieuni_dma_read(devOffset=0x%lx, dataSize=0x%lx)\n", devOffset, dataSize);

  if(!dev->memmory_base2) {
    PDEBUG(dev->name, "pcieuni_dma_read: ERROR: DMA BAR not mapped!\n");
    return -EFAULT;
  }

  // Loop until data is read
  for(; !IS_ERR(prevBuffer) && (dataRead < dmaSize);) {
    if(retVal) {
      nextBuffer = ERR_PTR(retVal);
    }
    else {
      // if there is more data to be requested from device
      if(dataReq < dmaSize) {
        // Find and reserve target buffer
        nextBuffer = pcieuni_bufferList_get_free(&mdev->dmaBuffers);
        if(!IS_ERR(nextBuffer)) {
          // prepare buffer to accept DMA data from device
          dma_sync_single_for_device(
              &dev->pcieuni_pci_dev->dev, nextBuffer->dma_handle, (size_t)nextBuffer->size, DMA_FROM_DEVICE);

          // request read of next data chunk
          nextBuffer->dma_size = min(dmaSize - dataReq, nextBuffer->size);
          nextBuffer->dma_offset = devOffset + dataReq;
          retVal = pcieuni_start_dma_read(dev, nextBuffer);
          if(retVal) {
            // make buffer available for next DMA request
            dma_sync_single_for_cpu(
                &dev->pcieuni_pci_dev->dev, nextBuffer->dma_handle, (size_t)nextBuffer->size, DMA_FROM_DEVICE);
            pcieuni_bufferList_set_free(&mdev->dmaBuffers, nextBuffer);
            nextBuffer = ERR_PTR(retVal);
          }
        }

        if(!IS_ERR(nextBuffer)) {
          dataReq += nextBuffer->dma_size; // add to total data requested
        }
      }
      else {
        nextBuffer = 0;
      }
    }

    // if data read was requested for prevBuffer
    if(prevBuffer) {
      // wait until data read is completed (device irq)
      retVal = pcieuni_wait_dma_read(mdev, prevBuffer);
      if(!retVal) {
        // copy data to proper offset in the target user-space buffer
        dma_sync_single_for_cpu(
            &dev->pcieuni_pci_dev->dev, prevBuffer->dma_handle, (size_t)prevBuffer->size, DMA_FROM_DEVICE);
        if(copy_to_user(
               userBuffer + dataRead, (void*)prevBuffer->kaddr, min(prevBuffer->dma_size, dataSize - dataRead))) {
          retVal = -EFAULT;
        }
        else {
          // add to total data read
          dataRead += prevBuffer->dma_size;
        }
      }
      // mark buffer available
      pcieuni_bufferList_set_free(&mdev->dmaBuffers, prevBuffer);
    }

    prevBuffer = nextBuffer;
  }

  if(IS_ERR(prevBuffer)) {
    retVal = retVal ? retVal : PTR_ERR(prevBuffer);
  }

  PDEBUG(
      dev->name, "pcieuni_dma_read(devOffset=0x%lx, dataSize=0x%lx): Return code(%i)\n", devOffset, dataSize, retVal);

  return retVal;
}

/**
 * @brief IOCTL handler for DMA related commnads
 *
 * @param filp      Device file
 * @param cmd_p     IOCTL command
 * @param arg_p     IOCLT arguments
 * @param pcieuni_cdev_m
 * @retval 0    success
 * @retval <0   error code
 */
long pcieuni_ioctl_dma(struct file* filp, unsigned int* cmd_p, unsigned long* arg_p, pcieuni_cdev* pcieuni_cdev_m) {
  unsigned int cmd;
  unsigned long arg;
  pid_t cur_proc = 0;
  int minor = 0;
  int d_num = 0;
  int retval = 0;
  int err = 0;
  struct pci_dev* pdev;

  int size_time;
  int io_dma_size;
  device_ioctrl_time time_data;
  device_ioctrl_dma dma_data;

  module_dev* module_dev_pp;
  pcieuni_dev* dev = filp->private_data;
  module_dev_pp = pcieuni_get_mdev(dev);

  cmd = *cmd_p;
  arg = *arg_p;
  size_time = sizeof(device_ioctrl_time);
  io_dma_size = sizeof(device_ioctrl_dma);
  minor = dev->dev_minor;
  d_num = dev->dev_num;
  cur_proc = current->group_leader->pid;
  pdev = (dev->pcieuni_pci_dev);

  if(!dev->dev_sts) {
    printk(KERN_DEBUG "pcieuni: no device %d\n", dev->dev_num);
    retval = -EFAULT;
    return retval;
  }

  PDEBUG(dev->name, "pcieuni_ioctl_dma(nr=%d )", _IOC_NR(cmd));

  /*
   * the direction is a bitmask, and VERIFY_WRITE catches R/W
   * transfers. `Type' is user-oriented, while
   * access_ok is kernel-oriented, so the concept of "read" and
   * "write" is reversed
   */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
  if(_IOC_DIR(cmd) & _IOC_READ)
    err = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
  else if(_IOC_DIR(cmd) & _IOC_WRITE)
    err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
#else
  err = !access_ok((void __user*)arg, _IOC_SIZE(cmd));
#endif
  if(err) return -EFAULT;

  if(mutex_lock_interruptible(&dev->dev_mut)) return -ERESTARTSYS;

  switch(cmd) {
    case PCIEUNI_GET_DMA_TIME:
      retval = 0;

      module_dev_pp->dma_start_time.tv_sec += (long)dev->slot_num;
      module_dev_pp->dma_stop_time.tv_sec += (long)dev->slot_num;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
      module_dev_pp->dma_start_time.tv_usec += (long)dev->brd_num;
      module_dev_pp->dma_stop_time.tv_usec += (long)dev->brd_num;
      time_data.start_time = module_dev_pp->dma_start_time;
      time_data.stop_time = module_dev_pp->dma_stop_time;
#else
      module_dev_pp->dma_start_time.tv_nsec += (long)dev->brd_num * NSEC_PER_USEC;
      module_dev_pp->dma_stop_time.tv_nsec += (long)dev->brd_num * NSEC_PER_USEC;
      time_data.start_time.tv_sec = module_dev_pp->dma_start_time.tv_sec;
      time_data.stop_time.tv_sec = module_dev_pp->dma_stop_time.tv_sec;
      time_data.start_time.tv_usec = module_dev_pp->dma_start_time.tv_nsec / NSEC_PER_USEC;
      time_data.stop_time.tv_usec = module_dev_pp->dma_stop_time.tv_nsec / NSEC_PER_USEC;
#endif
      if(copy_to_user((device_ioctrl_time*)arg, &time_data, (size_t)size_time)) {
        retval = -EIO;
        mutex_unlock(&dev->dev_mut);
        return retval;
      }
      break;

    case PCIEUNI_READ_DMA: {
      // Copy DMA transfer arguments into workqeue-data structure
      if(copy_from_user(&dma_data, (void*)arg, io_dma_size)) {
        mutex_unlock(&dev->dev_mut);
        return -EFAULT;
      }

      retval = pcieuni_dma_read(dev, dma_data.dma_offset, dma_data.dma_size, (void*)arg);
      break;
    }

    default:
      return -ENOTTY;
      break;
  }
  mutex_unlock(&dev->dev_mut);
  return retval;
}
