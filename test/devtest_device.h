#ifndef DEVTEST_DEVICE
#define DEVTEST_DEVICE

#include <string>
#include <unistd.h>
#include <pthread.h>
#include "pciedev_io.h"

using namespace std;

class IDevice
{
public:
    virtual bool StatusOk() const = 0;
    virtual string Error() const = 0;
    virtual int DmaRead(device_ioctrl_dma& dma_rw, char* buffer) const = 0;
    virtual int KbufDmaRead(device_ioctrl_dma& dma_rw, char* buffer) const = 0;
    virtual int StartKRingDmaRead(int offset, int bytes, int bytesPerCall) = 0;
    virtual int WaitKRingDmaRead(char* buffer, int offset, int bytes, int bytesPerCall) const = 0;
    
    virtual ~IDevice() {};
};

class TDeviceMock : public IDevice
{
public:
    virtual bool StatusOk() const { return true; }
    virtual string Error() const { return ""; }
    virtual int DmaRead(device_ioctrl_dma& dma_rw, char* buffer) const 
    { 
        usleep(10);
        return 0; 
    }

    virtual int KbufDmaRead(device_ioctrl_dma& dma_rw, char* buffer) const 
    { 
        usleep(10);
        return 0; 
    }
    
    virtual int StartKRingDmaRead(int offset, int bytes, int bytesPerCall)
    {
        return 0;
    }
    
    virtual int WaitKRingDmaRead(char* buffer, int offset, int bytes, int bytesPerCall) const
    {
        usleep(1000);
        return bytes; 
    }
    
};


class TDevice : public IDevice
{
    struct TReadReq
    {
        TReadReq(int handle, int offset, int bytes, int bytesPerCall) 
        :   Handle(handle),
            Offset(offset),
            Bytes(bytes),
            BytesPerCall(bytesPerCall)
        {}
        
        const int Handle;
        const int Offset;
        const int Bytes;
        const int BytesPerCall;
    };
    
public:
    TDevice(string deviceFile);
    
    int Handle() const;
    virtual bool   StatusOk() const;
    virtual string Error() const;
    virtual int DmaRead(device_ioctrl_dma& dma_rw, char* buffer) const;
    virtual int KbufDmaRead(device_ioctrl_dma& dma_rw, char* buffer) const ;
    virtual int StartKRingDmaRead(int offset, int bytes, int bytesPerCall);
    virtual int WaitKRingDmaRead(char* buffer, int offset, int bytes, int bytesPerCall) const;
    
private:
    static    void* DoStartKRingDmaRead(void* readReq);
    
    string    fFile;      /**< Name of the device file node in /dev.  */
    int       fHandle;    /**< Device file descriptor.                */
    void*     fDevice;    /**< Device private context.                */
    pthread_t fReadReqThread;
};

#endif