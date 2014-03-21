#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <memory>

#include "devtest_device.h"
#include "devtest_timer.h"
#include "pciedev_io.h"


using namespace std;

enum TMainMenuOption
{
    MAIN_MENU_INVALID  = -1,
    MAIN_MENU_DMA_READ_SINGLE,
    MAIN_MENU_DMA_READ_CHUNK,
    MAIN_MENU_DMA_READ_512MB_4MB,
    MAIN_MENU_DMA_READ_512MB_1MB,
    MAIN_MENU_KBUF_DMA_READ_SINGLE,
    MAIN_MENU_KBUF_DMA_READ_CHUNK,
    MAIN_MENU_KBUF_DMA_READ_512MB_4MB,
    MAIN_MENU_KBUF_DMA_READ_512MB_1MB,
    MAIN_MENU_KRING_DMA_READ_512MB_4MB,
    MAIN_MENU_EXIT
};

TMainMenuOption GetMainMenuChoice()
{
    map<TMainMenuOption, string> options;
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_EXIT, "Exit"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_SINGLE,          "DMA - Single read operation"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_CHUNK,           "DMA - Read device memory chunk"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_512MB_4MB,       "DMA - Read 512MB at 4MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_512MB_1MB,       "DMA - Read 512MB at 1MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_SINGLE,     "KBUF DMA - Single read operation"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_CHUNK,      "KBUF DMA - Read device memory chunk"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_512MB_4MB,  "KBUF DMA - Read 512MB at 4MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_512MB_1MB,  "KBUF DMA - Read 512MB at 1MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_DMA_READ_512MB_4MB, "KRING DMA - Read 512MB at 1MB per read"));
    

    cout << "********** Main Menu **********" << endl;
    map<TMainMenuOption, string>::const_iterator iter;
    for (iter = options.begin(); iter != options.end(); ++iter)
    {
        cout << "(" << iter->first << ") " << iter->second << endl;
    }
    int choice;
    cin >> choice;

    for (iter = options.begin(); iter != options.end(); ++iter)
    {
        if (iter->first == choice) break;
    }

    return iter == options.end() ? MAIN_MENU_INVALID : iter->first;
}

int GetOffsetChoice()
{
    cout << "**** Device offset (0):" ;
    int choice(0);
    cin >> choice;
    return choice;
}

int GetNumSamplesChoice()
{
    cout << "**** Number of int samples (1048576):" ;
    int choice(0);
    cin >> choice;
    if (choice <= 0) choice = 1024*1024;
    return choice;
}

int GetSamplesPerOperationChoice()
{
    cout << "**** Max number of int samples per system call (1048576):" ;
    int choice(0);
    cin >> choice;
    if (choice <= 0) choice = 1024*1024;
    return choice;    
}


// void DmaRead(IDevice* device, int offset, int samples, int samplesPerCall)
// {
//     vector<int> buffer;
//     buffer.resize(samplesPerCall + DMA_DATA_OFFSET);
//     
//     cout << "********** DmaRead ***********" << endl;
//     cout << "*** " << hex << "DMA_OFFSET: " << offset << ", DMA_SIZE: " << sizeof(int) * samples << dec << endl;
//     cout << "*** Reading... " << endl;
//     int samplesRead(0);
//     
//     int code(0);
//     int samplesToRead(0);
//     TTimer timeStart;
//     for (; samples > 0 ; )
//     {
//         samplesToRead = min<int>(samples, samplesPerCall);
//         
//         static device_ioctrl_dma dma_rw;
//         dma_rw.dma_cmd     = 0;
//         dma_rw.dma_pattern = 0; 
//         dma_rw.dma_size    = sizeof(int) * samplesToRead;
//         dma_rw.dma_offset  = offset;
//         
//         code = device->DmaRead(dma_rw, &buffer[0]);
//         if (code != 0) break;
//         
//         samplesRead += samplesToRead;
//         samples     -= samplesToRead;
//         offset      += sizeof(int) * samplesToRead; 
//     }
//     TTimer timeEnd;
//     
// 
//     if (code == 0)
//     {
//         cout << "*** Finished! " << endl;
//     }
//     else
//     {
//         cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
//         cout << "*** Read ERROR: " << code << endl; 
//         cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
//     }
//     
//     TTimer timeDiff = timeEnd - timeStart;
//     cout << "*** " << endl;
//     cout << "*** Total samples read: " << samplesRead << endl;
//     cout << "*** Real time: " << setw(10) << timeDiff.RealTime() << " us";
//     cout << " (Speed = " << samplesRead * sizeof(int) / timeDiff.RealTime() << " MB/s)" << endl;
//     cout << "*** CPU time:  " << setw(10) << timeDiff.CpuTime()   << " us" << endl;  
//     //cout << " (Speed = " << 1000 * samplesRead * sizeof(int) / timeDiff.CpuTime() << " kB/s)" << endl;
//     cout << "******************************" << endl;
// }

void DmaRead(IDevice* device, int offset, int bytes, int bytesPerCall)
{
    vector<char> buffer;
    buffer.resize(bytes + DMA_DATA_OFFSET_BYTE);
    
    cout << "********** DmaRead ***********" << endl;
    cout << "*** " << hex << "DMA_OFFSET: " << offset << ", DMA_SIZE: " << bytes << dec << endl;
    cout << "*** Reading... " << endl;
    int bytesRead(0);
    
    int code(0);
    int bytesToRead(0);
    TTimer timeStart;
    for (; bytes > 0 ; )
    {
        bytesToRead = min<int>(bytes, bytesPerCall);
        
        static device_ioctrl_dma dma_rw;
        dma_rw.dma_cmd     = 0;
        dma_rw.dma_pattern = 0; 
        dma_rw.dma_size    = bytesToRead;
        dma_rw.dma_offset  = offset;
        
        code = device->DmaRead(dma_rw, &buffer[0]);
        if (code != 0) break;
                   
        bytesRead += bytesToRead;
        bytes     -= bytesToRead;
        offset    += bytesToRead; 
    }
    TTimer timeEnd;
    
    if (code == 0)
    {
        cout << "*** Finished! " << endl;
    }
    else
    {
        cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
        cout << "*** Read ERROR: " << code << endl; 
        cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
    }
    
    TTimer timeDiff = timeEnd - timeStart;
    cout << "*** " << endl;
    cout << "*** Total bytes read: " << bytesRead << endl;
    cout << "*** Real time: " << setw(10) << timeDiff.RealTime() << " us";
    cout << " (Speed = " << bytesRead / timeDiff.RealTime() << " MB/s)" << endl;
    cout << "*** CPU time:  " << setw(10) << timeDiff.CpuTime()   << " us" << endl;  
    //cout << " (Speed = " << 1000 * samplesRead * sizeof(int) / timeDiff.CpuTime() << " kB/s)" << endl;
    cout << "******************************" << endl;
}

void KbufDmaRead(IDevice* device, int offset, int bytes, int bytesPerCall)
{
    vector<char> buffer;
    buffer.resize(bytes + DMA_DATA_OFFSET_BYTE);
    
    cout << "********** KbufDmaRead ***********" << endl;
    cout << "*** " << hex << "DMA_OFFSET: " << offset << ", DMA_SIZE: " << bytes << dec << endl;
    cout << "*** Reading... " << endl;
    int bytesRead(0);
    
    int code(0);
    int bytesToRead(0);
    TTimer timeStart;
    for (; bytes > 0 ; )
    {
        bytesToRead = min<int>(bytes, bytesPerCall);
        
        static device_ioctrl_dma dma_rw;
        dma_rw.dma_cmd     = 0;
        dma_rw.dma_pattern = 0; 
        dma_rw.dma_size    = bytesToRead;
        dma_rw.dma_offset  = offset;
        
        code = device->KbufDmaRead(dma_rw, &buffer[bytesRead]);
        if (code != 0) break;
        
        bytesRead += bytesToRead;
        bytes     -= bytesToRead;
        offset    += bytesToRead; 
    }
    TTimer timeEnd;
    
    if (code == 0)
    {
        cout << "*** Finished! " << endl;
    }
    else
    {
        cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
        cout << "*** Read ERROR: " << code << endl; 
        cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
    }
    
    TTimer timeDiff = timeEnd - timeStart;
    cout << "*** " << endl;
    cout << "*** Total bytes read: " << bytesRead << endl;
    cout << "*** Real time: " << setw(10) << timeDiff.RealTime() << " us";
    cout << " (Speed = " << bytesRead / timeDiff.RealTime() << " MB/s)" << endl;
    cout << "*** CPU time:  " << setw(10) << timeDiff.CpuTime()   << " us" << endl;  
    //cout << " (Speed = " << 1000 * samplesRead * sizeof(int) / timeDiff.CpuTime() << " kB/s)" << endl;
    cout << "******************************" << endl;
}

void KRingDmaRead(IDevice* device, int offset, int bytes, int bytesPerCall)
{
    vector<char> buffer;
    buffer.resize(bytes);
    
    cout << "********** KRingDmaRead ***********" << endl;
    cout << "*** " << hex << "DMA_OFFSET: " << offset << ", DMA_SIZE: " << bytes << dec << endl;
    cout << "*** Reading... " << endl;
    int bytesRead(0);
    
    TTimer timeStart;
    device->StartKRingDmaRead(offset, bytes, bytesPerCall);
    bytesRead = device->WaitKRingDmaRead(&buffer[0], offset, bytes, bytesPerCall);
    TTimer timeEnd;
    
    if (bytesRead > 0)
    {
        cout << "*** Finished! " << endl;
    }
    else
    {
        cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
        cout << "*** Read ERROR: " << bytesRead << endl; 
        cout << "*** !!!!!!!!!!!!!!!!!!" << endl;
    }
    
    TTimer timeDiff = timeEnd - timeStart;
    cout << "*** " << endl;
    cout << "*** Total bytes read: " << bytesRead << endl;
    cout << "*** Real time: " << setw(10) << timeDiff.RealTime() << " us";
    cout << " (Speed = " << bytesRead / timeDiff.RealTime() << " MB/s)" << endl;
    cout << "*** CPU time:  " << setw(10) << timeDiff.CpuTime()   << " us" << endl;  
    //cout << " (Speed = " << 1000 * samplesRead * sizeof(int) / timeDiff.CpuTime() << " kB/s)" << endl;
    cout << "******************************" << endl;
}

int main(int argc, char *argv[])
{
    unique_ptr<IDevice> device;
    
    if (argc < 2)
    {
        cout << "******************************" << endl;
        cout << "*** Using dummy device! " << endl;
        cout << "*** To work with real device you need to spcify character device file... " << endl;
        cout << "*** Usage:" << argv[0] << " <character device file>" << endl;
        cout << "******************************" << endl;
        device.reset(new (TDeviceMock));
    }
    else
    {
        device.reset(new TDevice(argv[1]));
    }
    
    if (!device->StatusOk())
    {
        cout << "Device ERROR:" << device->Error();
        return -1;
    }
    
    bool finished(false);
    while (!finished)
    {
        switch(GetMainMenuChoice())
        {
            case MAIN_MENU_EXIT:
            {
                finished = true;
                break;
            }
                
            case MAIN_MENU_DMA_READ_SINGLE:
            {
                int offset  = GetOffsetChoice();
                int samples = GetNumSamplesChoice();
                DmaRead(device.get(), offset, samples, samples);
                break;
            }

            case MAIN_MENU_DMA_READ_CHUNK:
            {
                int offset    = GetOffsetChoice();
                int samples   = GetNumSamplesChoice();
                int operation = GetSamplesPerOperationChoice();
                DmaRead(device.get(), offset, samples, operation);
                break;
            }
            
            case MAIN_MENU_DMA_READ_512MB_4MB:
            {
                DmaRead(device.get(), 0, 512*1024*1024, 4*1024*1024);
                break;
            }

            case MAIN_MENU_DMA_READ_512MB_1MB:
            {
                DmaRead(device.get(), 0, 512*1024*1024, 1024*1024);
                break;
            }

            case MAIN_MENU_KBUF_DMA_READ_SINGLE:
            {
                int offset  = GetOffsetChoice();
                int samples = GetNumSamplesChoice();
                KbufDmaRead(device.get(), offset, samples, samples);
                break;
            }
            
            case MAIN_MENU_KBUF_DMA_READ_CHUNK:
            {
                int offset    = GetOffsetChoice();
                int samples   = GetNumSamplesChoice();
                int operation = GetSamplesPerOperationChoice();
                KbufDmaRead(device.get(), offset, samples, operation);
                break;
            }
            
            case MAIN_MENU_KBUF_DMA_READ_512MB_4MB:
            {
                KbufDmaRead(device.get(), 0, 512*1024*1024, 4*1024*1024);
                break;
            }
            
            case MAIN_MENU_KBUF_DMA_READ_512MB_1MB:
            {
                KbufDmaRead(device.get(), 0, 512*1024*1024, 1024*1024);
                break;
            }

            case MAIN_MENU_KRING_DMA_READ_512MB_4MB:
            {
                KRingDmaRead(device.get(), 0, 512*1024*1024, 1024*1024);
                break;
            }
            
            default:
                cout << "ERROR! You have selected an invalid choice.";
                break;
        }
    }
    return 0;
}


