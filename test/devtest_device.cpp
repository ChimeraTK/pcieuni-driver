#include <fcntl.h>
#include "devtest_device.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>

void hex_dump(void* tgtBuffer, int size)
{
    cout << hex;
    for (int i = 0; i < size; i++)
    {
        if (i%8 == 0) cout << endl;
        int b = 0xFF & ((char*)tgtBuffer)[i];
        cout << " " << setw(2) << setfill('0') << b;
    }
    cout << dec << endl;
}

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

const string TDevice::Error() const
{
    return fError;
}

void TDevice::ResetStatus()
{
    fError.clear();
}

device_ioctrl_kbuf_info TDevice::KbufInfo()
{
    device_ioctrl_kbuf_info info;
    int code = ioctl(this->fHandle, PCIEDEV_KBUF_INFO, &info);
    if (code != 0) 
    {
        info.block_size = 0;
        info.num_blocks = 0;
        ostringstream stringStream;
        stringStream << "KbufInfo() ERROR! errno = " << errno << " (" << strerror(errno) << ")"; 
        this->fError = stringStream.str();
    }

    return info;
}

int TDevice::RegWrite(int bar, long offset, unsigned int data, long dataSize)
{
    struct device_rw rw;
    rw.offset_rw = offset;
    rw.data_rw   = 0;
    rw.data_rw   = data;
    if (dataSize = 1)
    {    
        rw.mode_rw   = RW_D8;
    }
    else if (dataSize = 2)
    {    
        rw.mode_rw   = RW_D16;
    }
    else if (dataSize = 4)
    {    
        rw.mode_rw   = RW_D32;
    }    
    else
    {    
        dataSize     = 1;
        rw.mode_rw   = RW_D8;
    }
    
    rw.barx_rw   = bar;
    rw.size_rw   = 0;
    rw.rsrvd_rw  = 0;
    
    int ret = write (fHandle, &rw, sizeof(device_rw));
    if (ret != sizeof(device_rw))
    {
        ostringstream stringStream;
        stringStream << "write() ERROR! errno = " << errno << " (" << strerror(errno) << ")"; 
        this->fError = stringStream.str();
        return -1;
    }    
    return 0;
}

int TDevice::RegRead(int bar, long offset, unsigned char* data, long dataSize)
{
    struct device_rw rw;
    rw.offset_rw = offset;
    rw.data_rw   = 0;
    rw.mode_rw   = RW_D32;
    rw.barx_rw   = bar;
    rw.size_rw   = 1;
    rw.rsrvd_rw  = 0;

    hex_dump(&rw, sizeof(device_rw));
    
    int ret = read(fHandle, &rw, sizeof(device_rw));
    if (ret != sizeof(device_rw))
    {
        ostringstream stringStream;
        stringStream << "read() ERROR! errno = " << errno << " (" << strerror(errno) << ")"; 
        this->fError = stringStream.str();
        return -1;
    }
    memcpy(data, &(rw.data_rw), 4);
    return 0;
}

int TDevice::ReadDma(device_ioctrl_dma& dma_rw, char* buffer)
{
    return this->Ioctl(PCIEDEV_READ_DMA, &dma_rw, buffer);
}

int TDevice::KbufReadDma(device_ioctrl_dma& dma_rw, char* buffer)
{
    return this->Ioctl(PCIEDEV_KBUF_READ_DMA, &dma_rw, buffer);
}

int TDevice::KringReadDma(device_ioctrl_dma& dma_rw, char* buffer)
{
    return this->Ioctl(PCIEDEV_KRING_READ_DMA, &dma_rw, buffer);
}

int TDevice::KringReadDmaNoCopy(device_ioctrl_dma& dma_rw, char* buffer)
{
    return this->Ioctl(PCIEDEV_KRING_READ_DMA_NOCOPY, &dma_rw, buffer);
}

int TDevice::RequestReadDma(int offset, int bytes, int bytesPerCall)
{
    TReadReq* req = new TReadReq(this, offset, bytes, bytesPerCall);
    
    int code = 1;
    
    for (int retry = 5; (retry > 0) && (code != 0) ; retry--)
    {
        code = pthread_create(&fReadReqThread, NULL, &DoRequestReadDma, req);
        pthread_detach(fReadReqThread);
    }
    
    if (code)
    {
        ostringstream stringStream;
        stringStream << " pthread(): ";
        stringStream << " ERROR! errno = " << code << " (" << strerror(code) << ")"; 
        fError = stringStream.str();
    }
}

void* TDevice::DoRequestReadDma(void* readReq)
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

        if (0 != req->Device->Ioctl(PCIEDEV_REQUEST_READ_DMA, &dma_rw))
        {
            break;
        }
        
        bytesRead += bytesToRead;
        bytes     -= bytesToRead;
    }
    
    delete req;
    
    return NULL;
}

int TDevice::Ioctl(long unsigned int req, device_ioctrl_dma* dma_rw, char* tgtBuffer)
{
    int code = 0;
    
    if (tgtBuffer == NULL)
    {
        code = ioctl(fHandle, req, dma_rw);
    }
    else
    {
        memcpy(tgtBuffer, dma_rw, sizeof(device_ioctrl_dma));
        code = ioctl(fHandle, req, tgtBuffer);
    }
    
    if (code != 0) 
    {
        ostringstream stringStream;
        stringStream << "Ioctl(req= " << _IOC_NR(req) << " ,dma_offset=" << dma_rw->dma_offset << ", dma_size=" << dma_rw->dma_size << ")";
        stringStream << " ERROR! errno = " << errno << " (" << strerror(errno) << ")"; 
        fError = stringStream.str();
    }
    
    return code;
}

int TDevice::WaitReadDma(char* buffer, int offset, int bytes, int bytesPerCall)
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
        
        if (0 != this->Ioctl(PCIEDEV_WAIT_READ_DMA, &dma_rw, &buffer[bytesRead]))
        {
            break;
        }
        
        bytesRead += bytesToRead;
        bytes     -= bytesToRead;
    }
    
    return bytesRead;
}

void TDevice::InitMMap(unsigned long bufsize)
{
    fMmapBufSize = bufsize;
    long offset  = 0;
    
    while (offset >= 0)
    {
        char* kbuf = (char*)mmap(NULL, fMmapBufSize, PROT_READ, MAP_SHARED, fHandle, offset);
        if (kbuf == MAP_FAILED)
        {
            offset = -1;
        }
        else
        {
            fMmapBufs.push_back(kbuf);
            offset += fMmapBufSize;
        }
    }
}

char* TDevice::GetMMapBuffer(unsigned long offset)
{
    int i = offset/fMmapBufSize;
    return i < fMmapBufs.size() ? fMmapBufs[i] : NULL;
}

void TDevice::ReleaseMMap()
{
    for (int i = 0; i < fMmapBufs.size(); i++)
    {
        munmap(fMmapBufs[i], fMmapBufSize);
    }
    
    fMmapBufs.clear();    
}

int TDevice::CollectMMapRead(char* buffer, int offset, int bytes, int bytesPerCall)
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
        
        if (0 != this->Ioctl(PCIEDEV_WAIT_MMAP_KBUF, &dma_rw))
        {
            break;
        }
        
        char* kbuf = this->GetMMapBuffer(dma_rw.dbuf_offset);
        
        if (kbuf) 
        {
            // This memcpy copies from kernel buffer to user space buffer. It is not necessary - here it simulates some data-processing 
            //memcpy(&buffer[bytesRead], kbuf, bytesPerCall);
            
            if (0 != this->Ioctl(PCIEDEV_RELEASE_MMAP_KBUF, &dma_rw))
            {
                break;
            }
            
            bytesRead += bytesToRead;
            bytes     -= bytesToRead;
        }
    }
    
    return bytesRead;
}
