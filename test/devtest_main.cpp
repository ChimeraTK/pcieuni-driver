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
        fBytesPerTest  = bytesPerTest;
        fNRuns       = nRuns;
        fBlockBytes  = blockBytes;
        fDoneBytes   = 0;
        fDevError    = "";
        fBuffer.resize(fBytesPerTest);
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
            
            if (fBytesPerTest >= 10240)
            {
                int blkIdx = (10240 + fBlockBytes - 1) / fBlockBytes;
                file << "***       Clock time to 10k: " << setw(10) << timePerTest * blkIdx / nBlocks << " us" << endl;
            }
            
            if (fBytesPerTest >= 102400)
            {
                int blkIdx = (102400 + fBlockBytes - 1) / fBlockBytes;
                file << "***       Clock time to 100k:" << setw(10) << timePerTest * blkIdx / nBlocks << " us" << endl;
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
    MAIN_MENU_DMA_READ_SINGLE,
    MAIN_MENU_DMA_READ_CHUNK,
    MAIN_MENU_DMA_READ_512MB_1MB,
    MAIN_MENU_DMA_READ_512MB_4MB,

    MAIN_MENU_KBUF_DMA_READ_SINGLE,
    MAIN_MENU_KBUF_DMA_READ_CHUNK,
    MAIN_MENU_KBUF_DMA_READ_512MB_4MB,
    MAIN_MENU_KBUF_DMA_READ_512MB_1MB,
    
    MAIN_MENU_KRING_DMA_READ_512MB_1MB,
    MAIN_MENU_KRING_DMA_READ_512MB_4MB,
    
    MAIN_MENU_KRING_MMAP_READ_512MB_1MB,
    MAIN_MENU_KRING_MMAP_READ_512MB_4MB,
    
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
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_SINGLE,          "DMA - Single read operation"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_CHUNK,           "DMA - Read device memory chunk"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_512MB_1MB,       "DMA - Read 512MB at 1MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_512MB_4MB,       "DMA - Read 512MB at 4MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_SINGLE,     "KBUF DMA - Single read operation"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_CHUNK,      "KBUF DMA - Read device memory chunk"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_512MB_4MB,  "KBUF DMA - Read 512MB at 4MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_512MB_1MB,  "KBUF DMA - Read 512MB at 1MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_DMA_READ_512MB_1MB, "KRING DMA - Read 512MB at 1MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_DMA_READ_512MB_4MB, "KRING DMA - Read 512MB at 4MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_MMAP_READ_512MB_1MB,"MMAP DMA - Read 512MB at 1MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_MMAP_READ_512MB_4MB,"MMAP DMA - Read 512MB at 4MB per read"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_READ_16MB,    "16 MB perfomance test: plain DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_KBUF_16MB,    "16 MB perfomance test: KBUF DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB,   "16 MB perfomance test: ASYNC DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB,    "16 MB perfomance test: MMAP DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_STRESS_TEST,              "Stress test"));
    
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

int GetOffsetChoice()
{
    cout << "**** Device offset (0):" ;
    int choice(0);
    cin >> choice;
    return choice;
}

int GetTotalBytesChoice()
{
    cout << "**** Bytes to transfer (4194304):" ;
    int choice(0);
    cin >> choice;
    if (choice <= 0) choice = 1024*1024*4;
    return choice;
}

int GetBlockBytesChoice()
{
    cout << "**** Size of transfer block (4194304):" ;
    int choice(0);
    cin >> choice;
    if (choice <= 0) choice = 1024*1024 * 4;
    return choice;    
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
        switch(GetMainMenuChoice())
        {
            case MAIN_MENU_EXIT:
            {
                finished = true;
                break;
            }
                
            case MAIN_MENU_DMA_READ_SINGLE:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                
                testLog.Init("KBUF_READ", &TestDmaRead, offset, bytes, 1, bytes);
                testLog.Run(device.get());
                break;
            }

            case MAIN_MENU_DMA_READ_CHUNK:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                long block  = GetBlockBytesChoice();
                
                testLog.Init("KBUF_READ", &TestDmaRead, offset, bytes, 1, block);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_DMA_READ_512MB_4MB:
            {
                testLog.Init("KBUF_READ", &TestDmaRead, 0,  512*1024*1024, 1, 4*1024*1024);
                testLog.Run(device.get());
                break;
            }

            case MAIN_MENU_DMA_READ_512MB_1MB:
            {
                testLog.Init("KBUF_READ", &TestDmaRead, 0,  512*1024*1024, 1, 1024*1024);
                testLog.Run(device.get());
                break;
            }

            case MAIN_MENU_KBUF_DMA_READ_SINGLE:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                
                testLog.Init("KBUF_READ", &TestKbufDmaRead, offset, bytes, 1, bytes);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_KBUF_DMA_READ_CHUNK:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                long block  = GetBlockBytesChoice();

                testLog.Init("KBUF_READ", &TestKbufDmaRead, offset, bytes, 1, block);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_KBUF_DMA_READ_512MB_4MB:
            {
                testLog.Init("KBUF_READ", &TestKbufDmaRead, 0,  512*1024*1024, 1, 4*1024*1024);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_KBUF_DMA_READ_512MB_1MB:
            {
                testLog.Init("KBUF_READ", &TestKbufDmaRead, 0, 512*1024*1024, 1, 1024*1024);
                testLog.Run(device.get());
                break;
            }

            case MAIN_MENU_KRING_DMA_READ_512MB_1MB:
            {
                testLog.Init("ASYNC_KBUF_READ", &TestAsyncDmaRead, 0, 512*1024*1024, 1, 1024*1024);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_KRING_DMA_READ_512MB_4MB:
            {
                testLog.Init("ASYNC_KBUF_READ", &TestAsyncDmaRead, 0, 512*1024*1024, 1, 4*1024*1024);
                testLog.Run(device.get());
                break;
            }

            case MAIN_MENU_KRING_MMAP_READ_512MB_1MB:
            {
                testLog.Init("MMAP_READ", &TestMMapRead, 0, 512*1024*1024, 1, 1*1024*1024);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_KRING_MMAP_READ_512MB_4MB:
            {
                testLog.Init("MMAP_READ", &TestMMapRead, 0, 512*1024*1024, 1, 4*1024*1024);
                testLog.Run(device.get());
                break;
            }

            case MAIN_MENU_PERFORMANCE_TEST_READ_16MB:
            {
                testLog.Init("KBUF_READ", &TestDmaRead, 0, 16*1024*1024, 64, -1);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_PERFORMANCE_TEST_KBUF_16MB:
            {
                testLog.Init("KBUF_READ", &TestKbufDmaRead, 0, 16*1024*1024, 64, -1);
                testLog.Run(device.get());
                break;
            }

            case MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB:
            {
                testLog.Init("KBUF_READ", &TestAsyncDmaRead, 0, 16*1024*1024, 64, -1);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB:
            {
                testLog.Init("KBUF_READ", &TestMMapRead, 0, 16*1024*1024, 64, -1);
                testLog.Run(device.get());
                break;
            }
            
            case MAIN_MENU_STRESS_TEST:
            {
//                StressTest(device.get());
                break;
            }       
            
            default:
                cout << "ERROR! You have selected an invalid choice.";
                break;
        }
    }
    return 0;
}


