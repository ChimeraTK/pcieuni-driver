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
    if (dataSize == 1)
    {    
        rw.mode_rw   = RW_D8;
    }
    else if (dataSize == 2)
    {    
        rw.mode_rw   = RW_D16;
    }
    else if (dataSize == 4)
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

int TDevice::KringReadDma(device_ioctrl_dma& dma_rw, char* buffer)
{
    return this->Ioctl(PCIEDEV_READ_DMA, &dma_rw, buffer);
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


