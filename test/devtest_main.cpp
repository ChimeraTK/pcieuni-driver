#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <memory>

#include "devtest_device.h"
#include "devtest_timer.h"
#include "pciedev_io.h"


using namespace std;


class TTest
{
public:
    typedef bool (TFn)(IDevice* device, TTest* test);
    
    TFn*         fTestFn;
    string       fTestName;
    long         fStartOffset;
    long         fBytesPerTest;
    int          fNRuns;
    long         fBlockBytes;
    long         fDoneBytes;
    string       fDevError;
    
    unique_ptr<TTimer> fTimeStart;
    unique_ptr<TTimer> fTimeEnd;
    
    vector<char> fBuffer;

    void Init(char* testName, TFn* testFn, long startOffset, long bytesPerTest, int nRuns, long blockBytes)
    {
        fTestName    = testName;
        fTestFn      = testFn;
        fStartOffset = startOffset;
        fBytesPerTest = bytesPerTest;
        fNRuns       = nRuns;
        fBlockBytes  = blockBytes;
        fDoneBytes   = 0;
        fDevError    = "";
        fBuffer.resize(fBytesPerTest);
        
        fill(fBuffer.begin(),fBuffer.end(), 0x42);
    }

    bool Run(IDevice* device)
    {
        if (this->fBlockBytes <= 0)
        {
            device_ioctrl_kbuf_info kbufInfo = device->KbufInfo();
            this->fBlockBytes = kbufInfo.block_size;
        }
            
        this->PrintHead(cout);
        this->fTestFn(device, this);
        this->PrintResult(cout);
    }

    void UpdateBytesDone(long newBytes)
    {
        fDoneBytes += newBytes;
    }
    
    void PrintHead(ostream& file)
    {
        file << endl << endl << endl;
        file << "**********************************************" << endl;
        file << "*** TEST: " << fTestName << endl;
        file << "***"                     << endl;
        file << "*** DMA offset         : " << hex << fStartOffset << dec << endl;
        file << "*** transfer size (kB) : " << fBytesPerTest/1024 << endl;
        file << "*** block size    (kB) : " << fBlockBytes/1024 << endl;
        file << "*** number of test runs: " << fNRuns << endl;
        file << "**********************************************" << endl;
    }
    
    void PrintInfo(ostream& file, const char* info)
    {
        file << "*** " << info << endl;
    }
    
    bool StatusOK()
    {
        return (fDoneBytes == fBytesPerTest * fNRuns) && fDevError.empty();
    }
    
    void PrintResult(ostream& file)
    {
        file << "**********************************************"  << endl;
        
        if (this->StatusOK())
        {
            TTimer timeTotal = *fTimeEnd   - *fTimeStart;
            file << "*** RESULT: OK!" << endl;
            file << "*** " << endl;
            file << "*** Total clock time:        " << setw(10) << timeTotal.RealTime() << " us" << " (Speed = " << fDoneBytes / timeTotal.RealTime() << " MB/s)" << endl;
            file << "*** Total CPU time:          " << setw(10) << timeTotal.CpuTime()   << " us" << endl;  
            file << "*** Total userspace time:    " << setw(10) << timeTotal.UserTime()   << " us" << endl;  
            file << "*** Total kernel time:       " << setw(10) << timeTotal.KernelTime()   << " us" << endl;  
            file << "*** " << endl;
            file << "*** Average test times       " << endl;
            file << "***       Clock time:        " << setw(10) << timeTotal.RealTime()/fNRuns << " us" << endl;
            file << "***       CPU time:          " << setw(10) << timeTotal.CpuTime()/fNRuns  << " us" << endl;  
            file << "***       kernel time:       " << setw(10) << timeTotal.KernelTime()/fNRuns  << " us" << endl;  
            file << "***       userspace time:    " << setw(10) << timeTotal.UserTime()/fNRuns  << " us" << endl;  
            
            long timePerTest = timeTotal.RealTime()/fNRuns;
            int  nBlocks = (fBytesPerTest + fBlockBytes - 1) / fBlockBytes;
            if (fBytesPerTest >= 1024)
            {
                int blkIdx = (1024 + fBlockBytes - 1) / fBlockBytes;
                file << "***       Clock time to 1k:  " << setw(10) << timePerTest * blkIdx / nBlocks << " us" << endl;
            }
            
            if (fBytesPerTest >= 1024*8)
            {
                int blkIdx = (1024*8 + fBlockBytes - 1) / fBlockBytes;
                file << "***       Clock time to 8k:  " << setw(10) << timePerTest * blkIdx / nBlocks << " us" << endl;
            }
            
            if (fBytesPerTest >= 1024*64)
            {
                int blkIdx = (1024*64 + fBlockBytes - 1) / fBlockBytes;
                file << "***       Clock time to 64k: " << setw(10) << timePerTest * blkIdx / nBlocks << " us" << endl;
            }
            
            if (fBytesPerTest >= 1024*1024)
            {
                int blkIdx = (1024*1024 + fBlockBytes - 1) / fBlockBytes;
                file << "***       Clock time to 1M:  " << setw(10) << timePerTest * blkIdx / nBlocks << " us" << endl;
            }
            
            file << "**********************************************"  << endl;
        }
        else
        {
            file << "*** RESULT: ERROR!" << endl;
            file << "***"                << endl;
            file << "*** Processed " << fDoneBytes << " of " << fBytesPerTest << " bytes" << endl; 
            file << "*** Device error: " << fDevError << endl;
            file << "**********************************************"  << endl;
        }
    }    
};

enum TMainMenuOption
{
    MAIN_MENU_INVALID  = -1,

    MAIN_MENU_REG_WRITE,
    MAIN_MENU_REG_READ,
    
    MAIN_MENU_DMA_READ_SINGLE,
    MAIN_MENU_KBUF_DMA_READ_SINGLE,
    
    MAIN_MENU_DMA_READ_CHUNK,
    MAIN_MENU_KBUF_DMA_READ_CHUNK,

    MAIN_MENU_KRING_DMA_READ_512MB,
    MAIN_MENU_MMAP_DMA_READ_512MB,
    
    MAIN_MENU_PERFORMANCE_TEST_READ_16MB,
    MAIN_MENU_PERFORMANCE_TEST_KBUF_16MB,
    MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB,
    MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB,
    
    MAIN_MENU_STRESS_TEST,
    MAIN_MENU_EXIT
};

TMainMenuOption GetMainMenuChoice()
{
    map<TMainMenuOption, string> options;
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_EXIT, "Exit"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_REG_WRITE,                  "Write 32bit register"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_REG_READ,                   "Read 32bit register"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_SINGLE,            "Single read operation: Simple DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_SINGLE,       "Single read operation: Single kernel buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_CHUNK,             "Read device memory chunk: Simple DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_CHUNK,        "Read device memory chunk: Single kernel buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_DMA_READ_512MB,       "Read 512MB: Kernel ring buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_MMAP_DMA_READ_512MB,        "Read 512MB: MMAP DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_READ_16MB, "16 MB perfomance test: Simple DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_KBUF_16MB, "16 MB perfomance test: Single kernel buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB,"16 MB perfomance test: Kernel ring buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB, "16 MB perfomance test: MMAP DMA"));
//    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_STRESS_TEST,                "Stress test"));
    
    cout << endl << endl << endl;
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

int GetBarChoice()
{
    cout << "**** Device BARx (0..5):" ;
    int choice(0);
    cin >> choice;
    return choice;
}

int GetOffsetChoice()
{
    cout << "**** Device offset (0):" ;
    int choice(0);
    cin >> choice;
    return choice;
}

vector<unsigned char> GetData32()
{
    cout << "**** Data to write:";
    
    vector<unsigned char> bytes;
    bytes.resize(4);
    
    for (int i=0; i <4; i++)
    {
        unsigned int byte;
        cout << "**** Byte" << i << " (hex):  ";
        cin >> hex >> byte;
        bytes[i] = byte & 0xFF;
    }
    return bytes;
}

int GetTotalBytesChoice()
{
    cout << "**** Size of transfer (kB):" ;
    int choice(0);
    cin >> choice;
    if (choice <= 0) choice = 4;
    return choice*1024;
}

int GetBlockBytesChoice()
{
    cout << "**** Size of transfer block (kB):" ;
    int choice(0);
    cin >> choice;
    if (choice <= 0) choice = 4;
    return choice*1024;    
}

int DumpBuffer(vector<char>& buffer)
{
    long size(64);
    
    while (size)
    {
        cout << endl << endl << endl;
        cout << "********** Dump buffer **********" << endl;
        cout << "Size (0 to finish): ";
        cin >> size;
        if (size)
        {
            long offset(0); 
            
            cout << "Offset            : ";
            cin >> offset;
            
            cout << hex;
            for (int i = 0; i < size; i++)
            {
                if (i%16 == 0) cout << endl << "*** ";
                unsigned int b = buffer[offset + i] & 0xFF;
                cout << " " <<  setw(2) << setfill('0') << b;
            }
            cout << dec << endl;
        }
    }
}


bool TestDmaRead(IDevice* device, TTest* test)
{
    int code = 0;
    
    test->fTimeStart.reset(new TTimer());
    for (int i = 0; (code == 0) && (i < test->fNRuns); i++)
    {
        
        long offset = test->fStartOffset;
        long bytes  = test->fBytesPerTest;
        long bytesRead = 0;
        
        for (; bytes > 0 ; )
        {
            long bytesToRead = min<int>(bytes, test->fBlockBytes);
            
            static device_ioctrl_dma dma_rw;
            dma_rw.dma_cmd     = 0;
            dma_rw.dma_pattern = 0; 
            dma_rw.dma_size    = bytesToRead;
            dma_rw.dma_offset  = offset;
            
            code = device->ReadDma(dma_rw, &test->fBuffer[bytesRead]);
            if (code != 0) break;
            
            bytesRead += bytesToRead;
            bytes     -= bytesToRead;
            offset    += bytesToRead; 
        }
        test->UpdateBytesDone(bytesRead);
    }
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
}


bool TestKbufDmaRead(IDevice* device, TTest* test)
{
    int code = 0;
    
    test->fTimeStart.reset(new TTimer());
    for (int i = 0; (code == 0) && (i < test->fNRuns); i++)
    {
        long offset    = test->fStartOffset;
        long bytes     = test->fBytesPerTest;
        long bytesRead = 0;
        
        for (; bytes > 0 ; )
        {
            long bytesToRead = min<int>(bytes, test->fBlockBytes);
            
            static device_ioctrl_dma dma_rw;
            dma_rw.dma_cmd     = 0;
            dma_rw.dma_pattern = 0; 
            dma_rw.dma_size    = bytesToRead;
            dma_rw.dma_offset  = offset;
            
            code = device->KbufReadDma(dma_rw, &test->fBuffer[bytesRead]);
            if (code != 0) 
            {
                break;
            }
            
            bytesRead += bytesToRead;
            bytes     -= bytesToRead;
            offset    += bytesToRead; 
        }
        test->UpdateBytesDone(bytesRead);
    }
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
    
    return true;
}

bool TestAsyncDmaRead(IDevice* device,  TTest* test)
{
    test->PrintInfo(cout, "Reading... ");
    
    test->fTimeStart.reset(new TTimer());
    for (int i = 0; i< test->fNRuns; i++)
    {
        long bytesRead = 0;
        if (!device->RequestReadDma(test->fStartOffset, test->fBytesPerTest, test->fBlockBytes))
        {
            bytesRead = device->WaitReadDma(&test->fBuffer[0], test->fStartOffset, test->fBytesPerTest, test->fBlockBytes);
        }
        test->UpdateBytesDone(bytesRead);    
    }
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
    
    return true;
}

bool TestMMapRead(IDevice* device, TTest* test)
{
    long totalBytesRead = 0;
    
    test->PrintInfo(cout, "Mapping driver memory buffers... ");
    device->InitMMap(test->fBlockBytes);
    
    test->PrintInfo(cout, "Reading... ");
    
    test->fTimeStart.reset(new TTimer());
    for (int i = 0; i< test->fNRuns; i++)
    {
        long bytesRead = 0;
        
        if (!device->RequestReadDma(test->fStartOffset, test->fBytesPerTest, test->fBlockBytes)) 
        {
            bytesRead = device->CollectMMapRead(&test->fBuffer[0], test->fStartOffset, test->fBytesPerTest, test->fBlockBytes);
        }
        
        test->UpdateBytesDone(bytesRead);    
    }
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
    
    test->PrintInfo(cout, "Clear memory mappings... ");
    device->ReleaseMMap();
    
    return true;
}

void StressTest(IDevice* device)
{
//     TTest testLog;
//     
//     for (int i= 0; i < 100; i++)
//     {
//         testLog.Init("KBUF_READ", &TestDmaRead, 0,  512*1024*1024, 1, 1*1024*1024);
//         testLog.PrintHead(cout);
//         TestDmaRead(device, &testLog);
//         if (!testLog.StatusOK()) { testLog.PrintResult(cout); return; }
//         
//         testLog.Init("KBUF_READ", 0, 512*1024*1024, 1, 1024*1024);
//         testLog.PrintHead(cout);
//         TestKbufDmaRead(device, &testLog);
//         if (!testLog.StatusOK()) { testLog.PrintResult(cout); return; }
//         
//         testLog.Init("KBUF_READ", 0, 512*1024*1024, 1, 4*1024*1024);
//         testLog.PrintHead(cout);
//         TestKbufDmaRead(device, &testLog);
//         if (!testLog.StatusOK()) { testLog.PrintResult(cout); return; }
//         
//         testLog.Init("ASYNC_KBUF_READ", 0, 512*1024*1024, 1, 1024*1024);
//         testLog.PrintHead(cout);
//         TestAsyncDmaRead(device, &testLog);
//         if (!testLog.StatusOK()) { testLog.PrintResult(cout); return; }
//         
//         testLog.Init("ASYNC_KBUF_READ", 0, 512*1024*1024, 1, 4*1024*1024);
//         testLog.PrintHead(cout);
//         TestAsyncDmaRead(device, &testLog);
//         if (!testLog.StatusOK()) { testLog.PrintResult(cout); return; }
//         
//         testLog.Init("MMAP_READ", 0, 512*1024*1024, 1, 1*1024*1024);
//         testLog.PrintHead(cout);
//         TestMMapRead(device, &testLog);
//         if (!testLog.StatusOK()) { testLog.PrintResult(cout); return; }
//         
//         testLog.Init("MMAP_READ", 0, 512*1024*1024, 1, 4*1024*1024);
//         testLog.PrintHead(cout);
//         TestMMapRead(device, &testLog);
//         if (!testLog.StatusOK()) { testLog.PrintResult(cout); return; }
//     }    
//     cout << "**********************************************"  << endl;
//     cout << "*** RESULT: OK!" << endl;
//     cout << "**********************************************"  << endl;    
}

int main(int argc, char *argv[])
{
    unique_ptr<IDevice> device;
    
    if (argc < 2)
    {
        cout << endl;
        cout << "******************************" << endl;
        cout << "*** Using dummy device! " << endl;
        cout << "*** To work with real device you need to spcify character device file... " << endl;
        cout << "*** Usage:" << argv[0] << " <character device file>" << endl;
        cout << "******************************" << endl;
        cout << endl;
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
    
    TTest testLog;

    bool finished(false);
    while (!finished)
    {
        TMainMenuOption option = GetMainMenuChoice();
        
        switch(option)
        {
            
            case MAIN_MENU_EXIT:
            {
                finished = true;
                break;
            }
            
            case MAIN_MENU_REG_WRITE:
            {   
                cout << "******** Write to device register ************"  << endl;
                int bar     = GetBarChoice();
                long offset = GetOffsetChoice();
                vector<unsigned char> data = GetData32();
                
                if (device.get()->RegWrite(bar, offset, &data[0], 4))
                {
                    cout << "*** Stauts: ERROR: " << device.get()->Error() << endl;
                }
                else
                {
                    cout << "*** Stauts: OK" <<  endl;
                    
                }
                cout << "**********************************************"  << endl;
                continue;
                break;
            }

            case MAIN_MENU_REG_READ:
            {
                cout << "******** Read from device register ***********"  << endl;
                int bar     = GetBarChoice();
                long offset = GetOffsetChoice();
                vector<unsigned char> data;
                data.resize(4);
                
                if (device.get()->RegRead(bar, offset, &(data[0]), 4))
                {
                    cout << "*** Stauts: ERROR: " << device.get()->Error() << endl;
                }
                else
                {
                    cout << "*** Stauts: OK" <<  endl;
                    cout << "*** Data: " << endl;
                    cout << "*** " << hex;
                    for (int i = 0; i < 4; i++)
                    {
                        unsigned int b = data[i] & 0xFF;
                        cout << " " <<  setw(2) << setfill('0') << b;
                    }
                    cout << dec << endl;
                }
                cout << "**********************************************"  << endl;
                continue;
                break;
            }
            
            case MAIN_MENU_DMA_READ_SINGLE:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                
                testLog.Init("Simple DMA read", &TestDmaRead, offset, bytes, 1, bytes);
                break;
            }
            
            case MAIN_MENU_KBUF_DMA_READ_SINGLE:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                
                testLog.Init("Kernel buffer read", &TestKbufDmaRead, offset, bytes, 1, bytes);
                break;
            }
            
            case MAIN_MENU_DMA_READ_CHUNK:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                long block  = GetBlockBytesChoice();
                testLog.Init("Simple DMA read", &TestDmaRead, offset, bytes, 1, block);
                break;
            }

            case MAIN_MENU_KBUF_DMA_READ_CHUNK:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                long block  = GetBlockBytesChoice();
                
                testLog.Init("Kernel buffer read", &TestKbufDmaRead, offset, bytes, 1, block);
                break;
            }

            case MAIN_MENU_KRING_DMA_READ_512MB:
                testLog.Init("Kernel ring buffer read", &TestAsyncDmaRead, 0, 512*1024*1024, 1, -1);
                break;
            
            case MAIN_MENU_MMAP_DMA_READ_512MB:
                testLog.Init("MMAP read", &TestMMapRead, 0, 512*1024*1024, 1, -1);
                break;

            case MAIN_MENU_PERFORMANCE_TEST_READ_16MB:
                testLog.Init("Simple DMA read", &TestDmaRead, 0, 16*1024*1024, 64, -1);
                break;
            
            case MAIN_MENU_PERFORMANCE_TEST_KBUF_16MB:
                testLog.Init("Kernel buffer read", &TestKbufDmaRead, 0, 16*1024*1024, 64, -1);
                break;

            case MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB:
                testLog.Init("Kernel ring buffer read", &TestAsyncDmaRead, 0, 16*1024*1024, 64, -1);
                break;
            
            case MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB:
                testLog.Init("MMAP read", &TestMMapRead, 0, 16*1024*1024, 64, -1);
                break;
            
            case MAIN_MENU_STRESS_TEST:
//                StressTest(device.get());
                break;
            
            default:
                cout << "ERROR! You have selected an invalid choice.";
                break;
        }
        
        if (!finished)
        {
            testLog.Run(device.get());
            DumpBuffer(testLog.fBuffer);
        }
    }
    return 0;
}


