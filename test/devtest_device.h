/**
 *  @file   devtest_device.h
 *  @brief  Declaration of device classes           
 */

#ifndef DEVTEST_DEVICE
#define DEVTEST_DEVICE

#include <iostream>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <iomanip>
#include <gpcieuni/pcieuni_io.h>

using namespace std;

/** 
 * @brief Base interface class that represents a PCI device 
 * 
 */
class IDevice
{
public:
    /**
     * @brief Gives descriptive device name.
     * 
     * @return std::string
     */
    virtual string Name() const = 0;

    /**
     * @brief Returns true if device had no errors
     * 
     * @return bool
     */
    virtual bool StatusOk() const = 0;
    
    /**
     * @brief Resets device error status.
     * 
     * @return void
     */
    virtual void ResetStatus() = 0;
    
    
    /**
     * @brief Returns error description
     * 
     * @return const std::string
     */
    virtual const string Error() const = 0;
    
    /**
     * @brief Write to PCI device register
     * 
     * @param bar       Traget BAR number
     * @param offset    Register offset within BAR
     * @param data      Data to write
     * @param dataSize  Size of data 
     * 
     * @retval 0 Success      
     * @retval 1 Failure      
     */
    virtual int RegWrite(int bar, long offset, unsigned int data, long dataSize) = 0; 
    
    
    /**
     * @brief Read from PCI device register
     * 
     * @param bar       Source BAR number
     * @param offset    Register offset within BAR
     * @param data      Target buffer
     * @param dataSize  Size of data 
     * 
     * @retval 0 Success      
     * @retval 1 Failure      
     */
    virtual int RegRead(int bar, long offset, unsigned char* data, long dataSize) = 0; 
    
    /**
     * @brief Read from device using DMA read IOCTL
     * 
     * @param dma_rw    DMA read request
     * @param buffer    Target buffer
     * 
     * @retval 0  Success
     * @retval <0 Error number
     */
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer) = 0;
    
    virtual ~IDevice() {};
};

/**
 * @brief Mock device class.
 * 
 * Does nothing. It is only useful for offline tool testing
 * 
 */
class TDeviceMock : public IDevice
{
public:
    TDeviceMock(string deviceName) { fName = deviceName; }
    virtual string Name() const { return fName; }
    virtual bool StatusOk() const { return true; }
    virtual const string Error() const { return string(); }
    virtual void ResetStatus() {}
    virtual int RegWrite(int bar, long offset, unsigned int data, long dataSize) 
    { 
        cout << fName << ": RegWrite(bar=" << bar << ", offset=" << hex << offset << ", data=" << data << dec << ")" << endl;
        return 0;
    }
    
    virtual int RegRead(int bar, long offset, unsigned char* data, long dataSize)
    { 
        cout << fName << ": RegRead(bar=" << bar << ", offset=" << hex << offset << ", data=" << data << dec << ")" << endl;
        return 0;
    }
    
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer) 
    { 
        cout << fName << ": KringReadDma(offset=" << hex << dma_rw.dma_offset << ", size=" << dma_rw.dma_size << dec << ")" << endl;
        usleep(100);
        return 0;
    }
    
private:
    string fName;
};

/**
 * @brief Implements interface to PCI device
 * 
 */
class TDevice : public IDevice
{
    
public:
    TDevice(string deviceFile);
    
    virtual string Name() const;
    virtual bool StatusOk() const;
    virtual const string Error() const;
    virtual void ResetStatus();
    virtual int RegWrite(int bar, long offset, unsigned int data, long dataSize); 
    virtual int RegRead(int bar, long offset, unsigned char* data, long dataSize); 
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer);
    virtual int Ioctl(long unsigned int req, device_ioctrl_dma* dma_rw, char* tgtBuffer = NULL);
private:
    
    string    fFile;        /**< Name of the device file node in /dev.  */
    int       fHandle;      /**< Device file descriptor.                */
    void*     fDevice;      /**< Device private context.                */
    string    fError;       /**< String describing device errors.       */
};


#endif
