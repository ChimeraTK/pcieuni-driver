#include <fcntl.h>
#include "devtest_device.h"
#include <sys/ioctl.h>
#include <cstring>


TDevice::TDevice(string deviceFile)
    : fFile(deviceFile)
{
    fHandle = open(fFile.c_str(), O_RDWR);
}

int TDevice::Handle() const
{ 
    return fHandle; 
}

bool TDevice::StatusOk() const
{
    return fHandle >= 0;
}

string TDevice::Error() const
{
    return "Invalid device";
}

int TDevice::DmaRead(device_ioctrl_dma& dma_rw, char* buffer) const
{
    memcpy(buffer, &dma_rw, sizeof(dma_rw));
    return ioctl(fHandle, PCIEDEV_READ_DMA, buffer);
}

int TDevice::KbufDmaRead(device_ioctrl_dma& dma_rw, char* buffer) const
{
    memcpy(buffer, &dma_rw, sizeof(dma_rw));
    return ioctl(fHandle, PCIEDEV_READ_KBUF_DMA, buffer);
}

int TDevice::StartKRingDmaRead(int offset, int bytes, int bytesPerCall)
{
    TReadReq* req = new TReadReq(this->Handle(), offset, bytes, bytesPerCall);
    pthread_create(&fReadReqThread, NULL, &DoStartKRingDmaRead, req);
    
    return 0;
}

void* TDevice::DoStartKRingDmaRead(void* readReq)
{
    TReadReq* req = static_cast<TReadReq*>(readReq);
    
    int bytes = req->Bytes;
    
    for (int bytesRead = 0; bytes > 0 ; )
    {
        int bytesToRead = min<int>(bytes, req->BytesPerCall);
        
        static device_ioctrl_dma dma_rw;
        dma_rw.dma_cmd     = 0;
        dma_rw.dma_pattern = 0; 
        dma_rw.dma_size    = bytesToRead;
        dma_rw.dma_offset  = req->Offset + bytesRead;

        int code = ioctl(req->Handle, PCIEDEV_READ_KRING_DMA, &dma_rw);
        if (code != 0) 
        {
            // TODO: hande error
            break;
        }
        
        bytesRead += bytesToRead;
        bytes     -= bytesToRead;
    }
    
    delete req;
}

int TDevice::WaitKRingDmaRead( char* buffer, int offset, int bytes, int bytesPerCall) const
{
    int bytesRead = 0;
    for (; bytes > 0 ; )
    {
        int bytesToRead = min<int>(bytes, bytesPerCall);
        
        static device_ioctrl_dma dma_rw;
        dma_rw.dma_cmd     = 0;
        dma_rw.dma_pattern = 0; 
        dma_rw.dma_size    = bytesToRead;
        dma_rw.dma_offset  = offset + bytesRead;
        
        char* offBuffer = &buffer[bytesRead];
        memcpy(offBuffer, &dma_rw, sizeof(dma_rw));
        int code = ioctl(fHandle, PCIEDEV_GET_KRING_DMA, offBuffer);
        if (code != 0) 
        {
            // TODO: hande error
            break;
        }
        
        bytesRead += bytesToRead;
        bytes     -= bytesToRead;
    }
    
    return bytesRead;
}
