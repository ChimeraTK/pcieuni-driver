#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <map>
#include <memory>

#include "devtest_device.h"
#include "devtest_timer.h"
#include "pciedev_io.h"
#include "devtest_test.h"


using namespace std;


enum TMainMenuOption
{
    MAIN_MENU_INVALID  = -1,

    MAIN_MENU_REG_WRITE_32,
    MAIN_MENU_REG_READ,
    
    MAIN_MENU_BOARD_SETUP,
    MAIN_MENU_BOARD_RESET,
    
    MAIN_MENU_DMA_READ_SINGLE,

    MAIN_MENU_DMA_READ_PERFORMANCE_512KB,
    MAIN_MENU_DMA_READ_PERFORMANCE_1MB,
    MAIN_MENU_DMA_READ_PERFORMANCE_16MB,
    MAIN_MENU_DMA_READ_PERFORMANCE_512KB_10HZ,
    MAIN_MENU_DMA_READ_PERFORMANCE_1MB_10HZ,
    MAIN_MENU_DMA_READ_PERFORMANCE_16MB_10HZ,
    
    MAIN_MENU_DMA_READ_PERFORMANCE_REPORT,
    
    MAIN_MENU_EXIT
};

TMainMenuOption GetMainMenuChoice()
{
    map<TMainMenuOption, string> options;
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_EXIT, "Exit"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_REG_WRITE_32,                    "Write 32bit register"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_REG_READ,                        "Read 32bit register"));

    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_BOARD_SETUP,                     "Setup board"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_BOARD_RESET,                     "Reset board"));
    
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_SINGLE,                 "DMA read device memory chunk"));    

    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_PERFORMANCE_512KB,      "Performance test: DMA read 512kB"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_PERFORMANCE_1MB,        "Performance test: DMA read 1MB"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_PERFORMANCE_16MB,       "Performance test: DMA read 16MB"));

    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_PERFORMANCE_512KB_10HZ, "Performance test (10Hz): DMA read 512kB"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_PERFORMANCE_1MB_10HZ,   "Performance test (10Hz): DMA read 1MB"));
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_PERFORMANCE_16MB_10HZ,  "Performance test (10Hz): DMA read 16MB"));
    
    options.insert(pair<TMainMenuOption, string>(MAIN_MENU_DMA_READ_PERFORMANCE_REPORT,     "Collect performace data."));
    
    cout << endl << endl << endl;
    cout << "********** Main Menu **********" << endl;
    map<TMainMenuOption, string>::const_iterator iter;
    for (iter = options.begin(); iter != options.end(); ++iter)
    {
        cout << "(" << iter->first << ") \t" << iter->second << endl;
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
            size_t size(64);
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
                for (size_t i = 0; i < size; i++)
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
            size_t size(256);
            if (size > buffer.size())
            {
                size = buffer.size();
            }
            
            cout << endl << endl << endl;
            cout << "********** Dump buffer **********" << endl;

            cout << hex;
            for (size_t i = 0; i < size; i++)
            {
                if (i%16 == 0) cout << endl << "*** ";
                unsigned int b = buffer[i] & 0xFF;
                cout << " " <<  setw(2) << setfill('0') << b;
            }
            cout << dec << endl;
        
        }
        
        if (analyse == 3)
        {
            size_t size(64*1024);
            
            if (size > buffer.size())
            {
                size = buffer.size();
            }
            
            cout << "*** Dump buffer to file dma_data.txt ***" << endl;
            fstream fs;
            fs.open ("dma_data.txt", fstream::out);
            
            for (size_t i = 0; i < size; i++)
            {
                if (i%16 == 0) fs << endl;
                unsigned int b = buffer[i] & 0xFF;
                fs << " " <<  setw(3) << setfill(' ') << b;
            }
            
            fs.close();
        }
    }
}


void TestKringDmaRead(IDevice* device, TTest* test)
{
    int code = 0;
    
    static device_ioctrl_dma dma_rw;
    dma_rw.dma_cmd     = 0;
    dma_rw.dma_pattern = 0; 
    dma_rw.dma_size    = test->fBytesPerTest;
    dma_rw.dma_offset  = test->fStartOffset;
    code = device->KringReadDma(dma_rw, &test->fBuffer[0]);
    test->UpdateStatus(code ? 0 : test->fBytesPerTest, device->Error());
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
        cout << "*** Usage:" << argv[0] << " <character device file>" << endl;
        cout << "******************************" << endl;
        cout << endl;
        return -1;
    }
    
    device.reset(new TDevice(argv[1]));
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
                unsigned int word_reset_n = 0x200;  // bar 0
                
                cout << "******** Setup device registers ************"  << endl;
                
                WriteWordReg(device.get(), 0, word_reset_n + 0x00, 1);
                
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
                break;
            }
            
            case MAIN_MENU_DMA_READ_SINGLE:
            {
                long offset = GetOffsetChoice();
                long bytes  = GetTotalBytesChoice();
                
                testLog.Init("DMA read from device memory", &TestKringDmaRead, offset, bytes, 1, 0);
                testLog.Run(device.get());
                DumpBuffer(testLog.fBuffer);
                
                break;
            }

            case MAIN_MENU_DMA_READ_PERFORMANCE_512KB:
                testLog.Init("DMA read performance test", &TestKringDmaRead, 0, 512*1024, 1000, 0);
                testLog.Run(device.get());
                testLog.PrintStat(cout);
                break;
                
            case MAIN_MENU_DMA_READ_PERFORMANCE_1MB:
                testLog.Init("DMA read performance test", &TestKringDmaRead, 0, 1024*1024, 1000, 0);
                testLog.Run(device.get());
                testLog.PrintStat(cout);
                break;

            case MAIN_MENU_DMA_READ_PERFORMANCE_16MB:
                testLog.Init("DMA read performance test", &TestKringDmaRead, 0, 16*1024*1024, 1000, 0);
                testLog.Run(device.get());
                testLog.PrintStat(cout);
                break;
                
            case MAIN_MENU_DMA_READ_PERFORMANCE_512KB_10HZ:
                testLog.Init("10Hz DMA read performance test", &TestKringDmaRead, 0, 512*1024, 1000, 100000);
                testLog.Run(device.get());
                testLog.PrintStat(cout);
                break;

            case MAIN_MENU_DMA_READ_PERFORMANCE_1MB_10HZ:
                testLog.Init("10Hz DMA read performance test", &TestKringDmaRead, 0, 1024*1024, 1000, 100000);
                testLog.Run(device.get());
                testLog.PrintStat(cout);
                break;

                
            case MAIN_MENU_DMA_READ_PERFORMANCE_16MB_10HZ:
                testLog.Init("10Hz DMA read performance test", &TestKringDmaRead, 0, 16*1024*1024, 1000, 100000);
                testLog.Run(device.get());
                testLog.PrintStat(cout);                
                break;

            case MAIN_MENU_DMA_READ_PERFORMANCE_REPORT:
                testLog.Init("DMA read 1000 * 512kB contiguously", &TestKringDmaRead, 0, 512*1024, 1000, 0);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout);                
                
                testLog.Init("DMA read 1000 * 1MB contiguously  ", &TestKringDmaRead, 0, 1024*1024, 1000, 0);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout, false);
                
                testLog.Init("DMA read 1000 * 512kB at 10Hz rate", &TestKringDmaRead, 0, 512*1024, 1000, 100000);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout, false);                
                
                testLog.Init("DMA read 1000 * 1MB at 10 Hz rate ", &TestKringDmaRead, 0, 1024*1024, 1000, 100000);
                testLog.Run(device.get(), true);
                testLog.PrintStat(cout, false);                
                
                break;
                
                
            default:
                cout << "ERROR! You have selected an invalid choice.";
                break;
        }
    }
    return 0;
}


