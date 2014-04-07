#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <map>
#include <memory>
#include <math.h>

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
    
    vector<unique_ptr<TTimer> > fStartTimers;
    vector<unique_ptr<TTimer> > fEndTimers;
    
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
        fStartTimers.resize(nRuns);
        fEndTimers.resize(nRuns);
        
        fill(fBuffer.begin(),fBuffer.end(), 0x42);
    }

    bool Run(IDevice* device, bool silent = false)
    {
        if (this->fBlockBytes <= 0)
        {
            device_ioctrl_kbuf_info kbufInfo = device->KbufInfo();
            this->fBlockBytes = kbufInfo.block_size;
        }
            
        if (!silent) this->PrintHead(cout);
        this->fTestFn(device, this);
        if (!silent) this->PrintResult(cout);
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
            
            int clkTimePerTest = timeTotal.RealTime()/fNRuns;
            int cpuTimePerTest = timeTotal.CpuTime()/fNRuns;
            int kerTimePerTest = timeTotal.KernelTime()/fNRuns;
            int usrTimePerTest = timeTotal.UserTime()/fNRuns;
            int clkTimeTo4k   = 0;
            int clkTimeTo16k  = 0;
            int clkTimeTo128k = 0;
            int clkTimeTo1M   = 0;
            
            file << "*** Average test times       " << endl;
            file << "***       Clock time:        " << setw(10) << clkTimePerTest << " us" << endl;
            file << "***       CPU time:          " << setw(10) << cpuTimePerTest  << " us" << endl;  
            file << "***       kernel time:       " << setw(10) << kerTimePerTest  << " us" << endl;  
            file << "***       userspace time:    " << setw(10) << usrTimePerTest  << " us" << endl;  
            
            long timePerTest = timeTotal.RealTime()/fNRuns;
            int  nBlocks = (fBytesPerTest + fBlockBytes - 1) / fBlockBytes;
            if (fBytesPerTest >= 1024*4)
            {
                int blkIdx  = (1024*4 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo4k = timePerTest * blkIdx / nBlocks;
                file << "***       Clock time to 4k:  " << setw(10) << clkTimeTo4k << " us" << endl;
            }
            
            if (fBytesPerTest >= 1024*16)
            {
                int blkIdx  = (1024*16 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo16k = timePerTest * blkIdx / nBlocks;
                file << "***       Clock time to 16k: " << setw(10) << clkTimeTo16k << " us" << endl;
            }
            
            if (fBytesPerTest >= 1024*128)
            {
                int blkIdx  = (1024*128 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo128k = timePerTest * blkIdx / nBlocks;
                file << "***       Clock time to 128k:" << setw(10) << clkTimeTo128k << " us" << endl;
            }
            
            if (fBytesPerTest >= 1024*1024)
            {
                int blkIdx  = (1024*1024 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo1M = timePerTest * blkIdx / nBlocks;
                file << "***       Clock time to 1M:  " << setw(10) << clkTimeTo1M << " us" << endl;
            }
            
            file << "**********************************************"  << endl;
            file << "*** " 
                 << clkTimePerTest << "\t" 
                 << cpuTimePerTest << "\t" 
                 << kerTimePerTest << "\t"
                 << clkTimeTo4k << "\t"
                 << clkTimeTo16k << "\t"
                 << clkTimeTo128k << "\t"
                 << clkTimeTo1M << endl;
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

    void PrintStat(ostream& file)
    {
        if (this->StatusOK())
        {
//             TTimer timeTotal = *fTimeEnd - *fTimeStart;
//             int clkTimePerTest = timeTotal.RealTime()/fNRuns;
//             int cpuTimePerTest = timeTotal.CpuTime()/fNRuns;
//             int kerTimePerTest = timeTotal.KernelTime()/fNRuns;
//             int usrTimePerTest = timeTotal.UserTime()/fNRuns;
//             long timePerTest = timeTotal.RealTime()/fNRuns;
//             int  nBlocks = (fBytesPerTest + fBlockBytes - 1) / fBlockBytes;
//             if (fBytesPerTest >= 1024*4)
//             {
//                 int blkIdx  = (1024*4 + fBlockBytes - 1) / fBlockBytes;
//                 clkTimeTo4k = timePerTest * blkIdx / nBlocks;
//             }
//             
//             if (fBytesPerTest >= 1024*16)
//             {
//                 int blkIdx  = (1024*16 + fBlockBytes - 1) / fBlockBytes;
//                 clkTimeTo16k = timePerTest * blkIdx / nBlocks;
//             }
//             
//             if (fBytesPerTest >= 1024*128)
//             {
//                 int blkIdx  = (1024*128 + fBlockBytes - 1) / fBlockBytes;
//                 clkTimeTo128k = timePerTest * blkIdx / nBlocks;
//             }
//             
//             if (fBytesPerTest >= 1024*1024)
//             {
//                 int blkIdx  = (1024*1024 + fBlockBytes - 1) / fBlockBytes;
//                 clkTimeTo1M = timePerTest * blkIdx / nBlocks;
//             }
            
            // calculate averages
            double clkTimePerTestS = 0; 
            double cpuTimePerTestS = 0;
            double kerTimePerTestS = 0;
            double usrTimePerTestS = 0;
            
            for (int i = 0; i < fEndTimers.size(); i++)
            {
                TTimer timeTotal = *fEndTimers[i] - *fStartTimers[i];
                
                clkTimePerTestS += timeTotal.RealTime();
                cpuTimePerTestS += timeTotal.CpuTime();
                kerTimePerTestS += timeTotal.KernelTime();
                usrTimePerTestS += timeTotal.UserTime();
            }
            clkTimePerTestS = clkTimePerTestS / fEndTimers.size();
            cpuTimePerTestS = cpuTimePerTestS / fEndTimers.size();
            kerTimePerTestS = kerTimePerTestS / fEndTimers.size();
            usrTimePerTestS = usrTimePerTestS / fEndTimers.size();
            
            // calculate deviations
            double clkTimePerTestD = 0; 
            double cpuTimePerTestD = 0;
            double kerTimePerTestD = 0;
            double usrTimePerTestD = 0;
            
            for (int i = 0; i < fEndTimers.size(); i++)
            {
                TTimer timeTotal = *fEndTimers[i] - *fStartTimers[i];
                clkTimePerTestD += (timeTotal.RealTime() - clkTimePerTestS) * (timeTotal.RealTime() - clkTimePerTestS);
                cpuTimePerTestD += (timeTotal.CpuTime()  - cpuTimePerTestS) * (timeTotal.CpuTime()  - cpuTimePerTestS);
                kerTimePerTestD += (timeTotal.KernelTime() - kerTimePerTestS) * (timeTotal.KernelTime() - kerTimePerTestS);
                usrTimePerTestD += (timeTotal.UserTime() - usrTimePerTestS) * (timeTotal.UserTime() - usrTimePerTestS);
            }
            clkTimePerTestD = 100*sqrt(clkTimePerTestD/fEndTimers.size())/clkTimePerTestS;
            cpuTimePerTestD = 100*sqrt(cpuTimePerTestD/fEndTimers.size())/cpuTimePerTestS;
            kerTimePerTestD = 100*sqrt(kerTimePerTestD/fEndTimers.size())/kerTimePerTestS;
            usrTimePerTestD = 100*sqrt(usrTimePerTestD/fEndTimers.size())/usrTimePerTestS;
            
            // calculate time to data
            int clkTimeTo4k   = 0;
            int clkTimeTo16k  = 0;
            int clkTimeTo128k = 0;
            int clkTimeTo1M   = 0;
            int nBlocks = (fBytesPerTest + fBlockBytes - 1) / fBlockBytes;
            if (fBytesPerTest >= 1024*4)
            {
                int blkIdx  = (1024*4 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo4k = clkTimePerTestS * blkIdx / nBlocks;
            }
            
            if (fBytesPerTest >= 1024*16)
            {
                int blkIdx  = (1024*16 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo16k = clkTimePerTestS * blkIdx / nBlocks;
            }
            
            if (fBytesPerTest >= 1024*128)
            {
                int blkIdx  = (1024*128 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo128k = clkTimePerTestS * blkIdx / nBlocks;
            }
            
            if (fBytesPerTest >= 1024*1024)
            {
                int blkIdx  = (1024*1024 + fBlockBytes - 1) / fBlockBytes;
                clkTimeTo1M = clkTimePerTestS * blkIdx / nBlocks;
            }
            
            
            file << fTestName << "\t"
                 << fBytesPerTest / clkTimePerTestS << "\t" 
                 << fixed << setprecision(0) << clkTimePerTestS << "\t" << fixed << setprecision(1) << clkTimePerTestD << "\t"
                 << fixed << setprecision(0) << cpuTimePerTestS << "\t" << fixed << setprecision(1) << cpuTimePerTestD << "\t"
                 << fixed << setprecision(0) << kerTimePerTestS << "\t" << fixed << setprecision(1) << kerTimePerTestD << "\t"
                 << fixed << setprecision(0) << usrTimePerTestS << "\t" << fixed << setprecision(1) << usrTimePerTestD << "\t"
                 << clkTimeTo4k << "\t"
                 << clkTimeTo16k << "\t"
                 << clkTimeTo128k << "\t"
                 << clkTimeTo1M <<  "\t" << endl;
        }
        else
        {
            file << "Device error: " << fDevError << endl;
        }
    }    
    
};

enum TMainMenuOption
{
    MAIN_MENU_INVALID  = -1,

    MAIN_MENU_REG_WRITE_32,
    MAIN_MENU_REG_READ,
    
    MAIN_MENU_BOARD_SETUP,
    MAIN_MENU_BOARD_RESET,
    
    MAIN_MENU_DMA_READ_SINGLE,
    MAIN_MENU_KBUF_DMA_READ_SINGLE,
    MAIN_MENU_KRING_DMA_READ_SINGLE,
    
    MAIN_MENU_DMA_READ_CHUNK,
    MAIN_MENU_KBUF_DMA_READ_CHUNK,

    MAIN_MENU_KRING_DMA_READ_512MB,
    MAIN_MENU_MMAP_DMA_READ_512MB,
    
    MAIN_MENU_PERFORMANCE_TEST_READ_16MB,
    MAIN_MENU_PERFORMANCE_TEST_KBUF_16MB,
    MAIN_MENU_PERFORMANCE_TEST_KRING_16MB,
    MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB,
    MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB,
    MAIN_MENU_PERFORMANCE_TEST_ALL_16MB,
    
    MAIN_MENU_STRESS_TEST,
    MAIN_MENU_EXIT
};

TMainMenuOption GetMainMenuChoice()
{
    map<TMainMenuOption, string> options;
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_EXIT, "Exit"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_REG_WRITE_32,               "Write 32bit register"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_REG_READ,                   "Read 32bit register"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_BOARD_SETUP,                "Setup board"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_BOARD_RESET,                "Reset board"));
    
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_SINGLE,            "Single read operation: Simple DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_SINGLE,       "Single read operation: Single kernel buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_DMA_READ_SINGLE,       "Single read operation: Kernel ring buffer DMA"));    
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_CHUNK,             "Read device memory chunk: Simple DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KBUF_DMA_READ_CHUNK,        "Read device memory chunk: Single kernel buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_KRING_DMA_READ_512MB,       "Read 512MB: Kernel ring buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_MMAP_DMA_READ_512MB,        "Read 512MB: MMAP DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_READ_16MB, "16 MB perfomance test: Simple DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_KBUF_16MB, "16 MB perfomance test: Single kernel buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_KRING_16MB,"16 MB perfomance test: Kernel ring buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB,"16 MB perfomance test: Async kernel ring buffer DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB, "16 MB perfomance test: MMAP DMA"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_PERFORMANCE_TEST_ALL_16MB,  "16 MB perfomance test: ALL"));
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
    cout << "**** Device offset (hex):" ;
    int choice(0);
    cin >> hex >> choice;
    cin >> dec;
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

void DumpBuffer(vector<char>& buffer)
{
    int analyse(1);
        
    while (analyse)
    {
        
        
        cout << "*** Analyze data (0 - no, 1 - print buffer (sz=256, off=0), 2 - print buffer, 3 - save to file (sz=64k, off=0)): " << endl;
        cin >> analyse;
        if (!analyse)
        {    
            return;
        }   
        
        if (analyse == 2)
        {
            long size(64);
            cout << endl << endl << endl;
            cout << "********** Dump buffer **********" << endl;
            cout << "Size: ";
            cin >> size;
            if (size)
            {
                if (size > buffer.size())
                {
                    size = buffer.size();
                }
                
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

        if (analyse == 1)
        {
            long size(256);
            if (size > buffer.size())
            {
                size = buffer.size();
            }
            
            cout << endl << endl << endl;
            cout << "********** Dump buffer **********" << endl;

            cout << hex;
            for (int i = 0; i < size; i++)
            {
                if (i%16 == 0) cout << endl << "*** ";
                unsigned int b = buffer[i] & 0xFF;
                cout << " " <<  setw(2) << setfill('0') << b;
            }
            cout << dec << endl;
        
        }
        
        if (analyse == 3)
        {
            long size(64*1024);
            
            if (size > buffer.size())
            {
                size = buffer.size();
            }
            
            cout << "*** Dump buffer to file dma_data.txt ***" << endl;
            fstream fs;
            fs.open ("dma_data.txt", fstream::out);
            
            for (int i = 0; i < size; i++)
            {
                if (i%16 == 0) fs << endl;
                unsigned int b = buffer[i] & 0xFF;
                fs << " " <<  setw(3) << setfill(' ') << b;
            }
            
            fs.close();
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
        
        test->fStartTimers[i].reset(new TTimer());
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
        test->fEndTimers[i].reset(new TTimer());
        
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
        
        test->fStartTimers[i].reset(new TTimer());
        
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
        test->fEndTimers[i].reset(new TTimer());
        
    }
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
    
    return true;
}

bool TestKringDmaRead(IDevice* device, TTest* test)
{
    int code = 0;
    
    test->fTimeStart.reset(new TTimer());
    for (int i = 0; (code == 0) && (i < test->fNRuns); i++)
    {
        test->fStartTimers[i].reset(new TTimer());

        static device_ioctrl_dma dma_rw;
        dma_rw.dma_cmd     = 0;
        dma_rw.dma_pattern = 0; 
        dma_rw.dma_size    = test->fBytesPerTest;
        dma_rw.dma_offset  = test->fStartOffset;

        code = device->KringReadDma(dma_rw, &test->fBuffer[0]);
        test->UpdateBytesDone(test->fBytesPerTest);
        test->fEndTimers[i].reset(new TTimer());
    }
    
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
    
    return true;
}

bool TestAsyncDmaRead(IDevice* device,  TTest* test)
{
    //test->PrintInfo(cout, "Reading... ");
    
    test->fTimeStart.reset(new TTimer());
    for (int i = 0; i< test->fNRuns; i++)
    {
        long bytesRead = 0;
        test->fStartTimers[i].reset(new TTimer());
        
        if (!device->RequestReadDma(test->fStartOffset, test->fBytesPerTest, test->fBlockBytes))
        {
            bytesRead = device->WaitReadDma(&test->fBuffer[0], test->fStartOffset, test->fBytesPerTest, test->fBlockBytes);
        }
        test->UpdateBytesDone(bytesRead);    
        test->fEndTimers[i].reset(new TTimer());
        
    }
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
    
    return true;
}

bool TestMMapRead(IDevice* device, TTest* test)
{
    long totalBytesRead = 0;
    
    //test->PrintInfo(cout, "Mapping driver memory buffers... ");
    device->InitMMap(test->fBlockBytes);
    
    //test->PrintInfo(cout, "Reading... ");
    
    test->fTimeStart.reset(new TTimer());
    for (int i = 0; i< test->fNRuns; i++)
    {
        long bytesRead = 0;
        test->fStartTimers[i].reset(new TTimer());
        
        if (!device->RequestReadDma(test->fStartOffset, test->fBytesPerTest, test->fBlockBytes)) 
        {
            bytesRead = device->CollectMMapRead(&test->fBuffer[0], test->fStartOffset, test->fBytesPerTest, test->fBlockBytes);
        }
        
        test->UpdateBytesDone(bytesRead);    
        test->fEndTimers[i].reset(new TTimer());
        
    }
    test->fTimeEnd.reset(new TTimer());
    test->fDevError = device->Error();
    
    //test->PrintInfo(cout, "Clear memory mappings... ");
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

void WriteByteReg(IDevice* device, int bar, long offset, unsigned int data)
{
    cout << "*** Writing " << hex << setw(2) << setfill('0') << data << " to bar " << bar << " offset " << offset << dec << endl; 
    if (device->RegWrite(bar, offset, data, 1))
    {
        cout << "*** Stauts: ERROR: " << device->Error() << endl;
    }
}

void WriteWordReg(IDevice* device, int bar, long offset, unsigned int data)
{
    cout << "*** Writing " << hex << setw(8) << setfill('0') << data << " to bar " << bar << " offset " << offset << dec << endl; 
    if (device->RegWrite(bar, offset, data, 4))
    {
        cout << "*** Stauts: ERROR: " << device->Error() << endl;
    }
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
            
            case MAIN_MENU_REG_WRITE_32:
            {   
                cout << "******** Write to device register ************"  << endl;
                int bar     = GetBarChoice();
                long offset = GetOffsetChoice();
                vector<unsigned char> data = GetData32();
                
                if (device.get()->RegWrite(bar, offset, *(reinterpret_cast<u_int*>(&data[0])), 4))
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

            case MAIN_MENU_BOARD_SETUP:
            {   
                unsigned int area_spi_div = 0x1000; // bar 0
                unsigned int word_clk_mux = 0x80;   // bar 0
                unsigned int word_clk_sel = 0x9C;   // bar 0
                unsigned int word_reset_n = 0x200;  // bar 0
                unsigned int area_spi_adc = 0x2000; // bar 0
                unsigned int word_adc_ena = 0x100;  // bar 0
                unsigned int word_timing_freq    = 0x20; // bar 1
                unsigned int word_timing_int_ena = 0x10; // bar 1
                unsigned int word_timing_trg_sel = 0x80; // bar 1
                unsigned int word_daq_enable     = 0x08; // bar 1
                
                cout << "******** Setup device registers ************"  << endl;
                WriteByteReg(device.get(), 0, area_spi_div + 0x45, 0x00);
                WriteByteReg(device.get(), 0, area_spi_div + 0x0A, 0x43);
                WriteByteReg(device.get(), 0, area_spi_div + 0x3C, 0x0C);
                WriteByteReg(device.get(), 0, area_spi_div + 0x3D, 0x0C);
                WriteByteReg(device.get(), 0, area_spi_div + 0x3E, 0x0C);
                WriteByteReg(device.get(), 0, area_spi_div + 0x3F, 0x0C);
                WriteByteReg(device.get(), 0, area_spi_div + 0x40, 0x02);
                WriteByteReg(device.get(), 0, area_spi_div + 0x41, 0x02);
                WriteByteReg(device.get(), 0, area_spi_div + 0x42, 0x02);
                WriteByteReg(device.get(), 0, area_spi_div + 0x43, 0x02);
                WriteByteReg(device.get(), 0, area_spi_div + 0x49, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x4B, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x4D, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x4F, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x51, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x53, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x55, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x57, 0x80);
                WriteByteReg(device.get(), 0, area_spi_div + 0x5A, 0x81);
                
                WriteWordReg(device.get(), 0, word_clk_mux + 0x00, 0);
                WriteWordReg(device.get(), 0, word_clk_mux + 0x01, 0);
                WriteWordReg(device.get(), 0, word_clk_mux + 0x02, 3);
                WriteWordReg(device.get(), 0, word_clk_mux + 0x03, 3);
                
                WriteWordReg(device.get(), 0, word_clk_sel + 0x00, 0); // 1

                WriteWordReg(device.get(), 0, word_reset_n + 0x00, 1);
                
                WriteByteReg(device.get(), 0, area_spi_adc + 0x00, 0x3C);
                WriteByteReg(device.get(), 0, area_spi_adc + 0x14, 0x41);
                WriteByteReg(device.get(), 0, area_spi_adc + 0x0D, 0x00);
                WriteByteReg(device.get(), 0, area_spi_adc + 0xFF, 0x01);

                WriteWordReg(device.get(), 0, word_adc_ena + 0x00, 1);

                WriteWordReg(device.get(), 1, word_timing_freq + 0x00, 81250000);
                WriteWordReg(device.get(), 1, word_timing_freq + 0x01, 0);
                WriteWordReg(device.get(), 1, word_timing_freq + 0x02, 0);
                WriteWordReg(device.get(), 1, word_timing_freq + 0x03, 0);
                WriteWordReg(device.get(), 1, word_timing_freq + 0x04, 8);
                WriteWordReg(device.get(), 1, word_timing_freq + 0x05, 8);
                WriteWordReg(device.get(), 1, word_timing_freq + 0x06, 8);
                WriteWordReg(device.get(), 1, word_timing_freq + 0x07, 0);

                WriteWordReg(device.get(), 1, word_timing_trg_sel + 0x00, 0);
                
                WriteWordReg(device.get(), 1, word_timing_int_ena + 0x00, 0xF1);

                WriteWordReg(device.get(), 1, word_daq_enable + 0x00, 0x02);
                
                
                continue;
                break;
            }

            case MAIN_MENU_BOARD_RESET:
            {   
                unsigned int area_spi_div = 0x1000; // bar 0
                unsigned int word_clk_mux = 0x80;   // bar 0
                unsigned int word_clk_sel = 0x9C;   // bar 0
                unsigned int word_reset_n = 0x200;  // bar 0
                unsigned int area_spi_adc = 0x2000; // bar 0
                unsigned int word_adc_ena = 0x100;  // bar 0
                unsigned int word_timing_freq    = 0x20; // bar 1
                unsigned int word_timing_int_ena = 0x10; // bar 1
                unsigned int word_timing_trg_sel = 0x80; // bar 1
                unsigned int word_daq_enable     = 0x08; // bar 1
                
                cout << "******** Setup device registers ************"  << endl;
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x45, 0x00);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x0A, 0x43);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x3C, 0x0C);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x3D, 0x0C);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x3E, 0x0C);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x3F, 0x0C);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x40, 0x02);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x41, 0x02);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x42, 0x02);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x43, 0x02);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x49, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x4B, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x4D, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x4F, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x51, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x53, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x55, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x57, 0x80);
//                 WriteByteReg(device.get(), 0, area_spi_div + 0x5A, 0x81);
//                 
//                 WriteWordReg(device.get(), 0, word_clk_mux + 0x00, 0);
//                 WriteWordReg(device.get(), 0, word_clk_mux + 0x01, 0);
//                 WriteWordReg(device.get(), 0, word_clk_mux + 0x02, 3);
//                 WriteWordReg(device.get(), 0, word_clk_mux + 0x03, 3);
//                 
//                 WriteWordReg(device.get(), 0, word_clk_sel + 0x00, 1);
                
                WriteWordReg(device.get(), 0, word_reset_n + 0x00, 1);
                
//                 WriteByteReg(device.get(), 0, area_spi_adc + 0x00, 0x3C);
//                 WriteByteReg(device.get(), 0, area_spi_adc + 0x14, 0x41);
//                 WriteByteReg(device.get(), 0, area_spi_adc + 0x0D, 0x00);
//                 WriteByteReg(device.get(), 0, area_spi_adc + 0xFF, 0x01);
//                 
//                 WriteWordReg(device.get(), 0, word_adc_ena + 0x00, 1);
//                 
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x00, 81250000);
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x01, 0);
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x02, 0);
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x03, 0);
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x04, 8);
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x05, 8);
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x06, 8);
//                 WriteWordReg(device.get(), 1, word_timing_freq + 0x07, 0);
//                 
//                 WriteWordReg(device.get(), 1, word_timing_trg_sel + 0x00, 0);
//                 
//                 WriteWordReg(device.get(), 1, word_timing_int_ena + 0x00, 0xF1);
//                 
//                 WriteWordReg(device.get(), 1, word_daq_enable + 0x00, 0x02);
                
                
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

            case MAIN_MENU_KRING_DMA_READ_SINGLE:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                
                testLog.Init("Kernel ring buffer read", &TestKringDmaRead, offset, bytes, 1, -1);
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

            case MAIN_MENU_PERFORMANCE_TEST_KRING_16MB:
                testLog.Init("Kernel buffer ring read", &TestKbufDmaRead, 0, 16*1024*1024, 64, -1);
                break;
                
            case MAIN_MENU_PERFORMANCE_TEST_ASYNC_16MB:
                testLog.Init("Kernel ring buffer read", &TestAsyncDmaRead, 0, 16*1024*1024, 64, -1);
                break;
            
            case MAIN_MENU_PERFORMANCE_TEST_MMAP_16MB:
                testLog.Init("MMAP read", &TestMMapRead, 0, 16*1024*1024, 64, -1);
                break;
            
            case MAIN_MENU_PERFORMANCE_TEST_ALL_16MB:
            {
                int nRuns = 200;
                
                cout << "Test name              " << "\t"
                << "MB/s" << "\t" 
                << "Clk(us)" << "\t" << fixed << setprecision(1) << "er(%)" << "\t"
                << "Cpu(us)" << "\t" << fixed << setprecision(1) << "er(%)" << "\t"
                << "Krn(us)" << "\t" << fixed << setprecision(1) << "er(%)" << "\t"
                << "Usr(us)" << "\t" << fixed << setprecision(1) << "er(%)" << "\t"
                << "us 4k" << "\t"
                << "us 16k" << "\t"
                << "us 128k" << "\t"
                << "us 1M" << "\t" << endl;

                testLog.Init("Simple DMA read        ", &TestDmaRead, 0, 16*1024*1024, nRuns, -1);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout);
                testLog.Init("Kernel buffer read     ", &TestKbufDmaRead, 0, 16*1024*1024, nRuns, -1);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout);
                testLog.Init("Kernel ring buffer read", &TestAsyncDmaRead, 0, 16*1024*1024, nRuns, -1);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout);
                testLog.Init("MMAP read              ", &TestMMapRead, 0, 16*1024*1024, nRuns, -1);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout);
                
                continue;
            }
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


