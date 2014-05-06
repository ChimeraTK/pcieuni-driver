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
    virtual int ReadDma(device_ioctrl_dma& dma_rw, char* buffer) = 0;
    virtual int KbufReadDma(device_ioctrl_dma& dma_rw, char* buffer) = 0;
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer) = 0;
    virtual int KringReadDmaNoCopy(device_ioctrl_dma& dma_rw, char* buffer) = 0;
    virtual int RequestReadDma(int offset, int bytes, int bytesPerCall) = 0;
    virtual int WaitReadDma(char* buffer, int offset, int bytes, int bytesPerCall)= 0;
    virtual void InitMMap(unsigned long bufsize) = 0;
    virtual void ReleaseMMap() = 0; 
    virtual int CollectMMapRead(char* buffer, int offset, int bytes, int bytesPerCall) = 0;
    
    virtual ~IDevice() {};
};

class TDeviceMock : public IDevice
{
public:
    virtual bool StatusOk() const { return true; }
    virtual const string Error() const { return ""; }
    virtual void ResetStatus() {};
    
    virtual device_ioctrl_kbuf_info KbufInfo()
    {
        device_ioctrl_kbuf_info info;
        info.block_size = 4*1024*1024;
        info.num_blocks = 2;
        return info;
    }
    
    virtual int RegWrite(int bar, long offset, unsigned int data, long dataSize)
    {
        return 0;
    }
    
    virtual int RegRead(int bar, long offset, unsigned char* data, long dataSize)
    {
        return 0;
    }    
    
    virtual int ReadDma(device_ioctrl_dma& dma_rw, char* buffer) 
    { 
        usleep(10);
        return 0; 
    }

    virtual int KbufReadDma(device_ioctrl_dma& dma_rw, char* buffer) 
    { 
        usleep(10);
        return 0; 
    }

    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer) 
    { 
        usleep(10);
        return 0; 
    }
    
    virtual int KringReadDmaNoCopy(device_ioctrl_dma& dma_rw, char* buffer) 
    { 
        usleep(10);
        return 0; 
    }
    
    virtual int RequestReadDma(int offset, int bytes, int bytesPerCall)
    {
        return 0;
    }
    
    virtual int WaitReadDma(char* buffer, int offset, int bytes, int bytesPerCall)
    {
        usleep(1000);
        return bytes; 
    }
    
    virtual void InitMMap(unsigned long bufsize) {}
    
    virtual void ReleaseMMap() {}
    
    virtual int CollectMMapRead(char* buffer, int offset, int bytes, int bytesPerCall) 
    {
        usleep(1000);
        return bytes; 
    }
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
    virtual int ReadDma(device_ioctrl_dma& dma_rw, char* buffer);
    virtual int KbufReadDma(device_ioctrl_dma& dma_rw, char* buffer);
    virtual int KringReadDma(device_ioctrl_dma& dma_rw, char* buffer);
    virtual int KringReadDmaNoCopy(device_ioctrl_dma& dma_rw, char* buffer);
    virtual int RequestReadDma(int offset, int bytes, int bytesPerCall);
    virtual int Ioctl(long unsigned int req, device_ioctrl_dma* dma_rw, char* tgtBuffer = NULL);
    virtual int WaitReadDma(char* buffer, int offset, int bytes, int bytesPerCall);
    virtual void InitMMap(unsigned long bufsize);
    virtual void ReleaseMMap(); 
    virtual int CollectMMapRead(char* buffer, int offset, int bytes, int bytesPerCall);
    
private:
    static void* DoRequestReadDma(void* readReq);
    char*  GetMMapBuffer(unsigned long offset);
    
    
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