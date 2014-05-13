#ifndef DEVTEST_DEVICE
#define DEVTEST_DEVICE

#include <string>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include "pciedev_io.h"

using namespace std;

class IDevice
{
public:
    virtual bool StatusOk() const = 0;
    virtual const string Error() const = 0;
    virtual void ResetStatus() = 0;
    virtual device_ioctrl_kbuf_info KbufInfo() = 0;
    virtual int RegWrite(int bar, long offset, unsigned int data, long dataSize) = 0; 
    virtual int RegRead(int bar, long offset, unsigned char* data, long dataSize) = 0; 
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer) = 0;
    
    virtual ~IDevice() {};
};

class TDevice : public IDevice
{
    struct TReadReq
    {
        TReadReq(TDevice* device, int offset, int bytes, int bytesPerCall) 
        :   Device(device),
            Offset(offset),
            Bytes(bytes),
            BytesPerCall(bytesPerCall)
        {}
        
        TDevice* const Device;
        const int Offset;
        const int Bytes;
        const int BytesPerCall;
    };
    
public:
    TDevice(string deviceFile);
    
    int Handle() const;
    virtual bool StatusOk() const;
    virtual const string Error() const;
    virtual void ResetStatus();
    virtual device_ioctrl_kbuf_info KbufInfo();
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
    device_ioctrl_kbuf_info fKbufInfo;

    unsigned long fMmapBufSize;    
    vector<char*> fMmapBufs;
};

#endif