#ifndef DEVTEST_DEVICE
#define DEVTEST_DEVICE

#include <iostream>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <iomanip>
#include "pciedev_io.h"

using namespace std;

class IDevice
{
public:
    virtual string Name() const = 0;
    virtual bool StatusOk() const = 0;
    virtual const string Error() const = 0;
    virtual void ResetStatus() = 0;
    virtual int RegWrite(int bar, long offset, unsigned int data, long dataSize) = 0; 
    virtual int RegRead(int bar, long offset, unsigned char* data, long dataSize) = 0; 
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer) = 0;
    
    virtual ~IDevice() {};
};

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

class TDevice : public IDevice
{
    
public:
    TDevice(string deviceFile);
    
    int Handle() const;
    virtual string Name() const;
    virtual bool StatusOk() const;
    virtual const string Error() const;
    virtual void ResetStatus();
    virtual int RegWrite(int bar, long offset, unsigned int data, long dataSize); 
    virtual int RegRead(int bar, long offset, unsigned char* data, long dataSize); 
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer);
    virtual int Ioctl(long unsigned int req, device_ioctrl_dma* dma_rw, char* tgtBuffer = NULL);
private:
    
    string    fFile;      /**< Name of the device file node in /dev.  */
    int       fHandle;    /**< Device file descriptor.                */
    void*     fDevice;    /**< Device private context.                */
    pthread_t fReadReqThread;
    string    fError;
    
    unsigned long fMmapBufSize;    
    vector<char*> fMmapBufs;
};


#endif