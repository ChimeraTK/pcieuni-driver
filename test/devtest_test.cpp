#include <iostream>
#include <fstream>
#include <iomanip>
#include <math.h>
#include "devtest_test.h"

void TTest::Init(const std::string& testName, TFn* testFn, long startOffset, long bytesPerTest, int nRuns, long runIntervalUs)
{
    fTestName      = testName;
    fTestFn        = testFn;
    fStartOffset   = startOffset;
    fBytesPerTest  = bytesPerTest;
    fNRuns         = nRuns;
    fRunIntervalUs = runIntervalUs;
    fBlockBytes    = 0;
    fDoneBytes     = 0;
    fDevError      = "";
    fBuffer.resize(fBytesPerTest);
    fStartTimers.resize(nRuns);
    fEndTimers.resize(nRuns);
    
    fill(fBuffer.begin(),fBuffer.end(), 0x42);
}

void TTest::Run(IDevice* device, bool silent)
{
    device->ResetStatus();
    if (this->fBlockBytes <= 0)
    {
        device_ioctrl_kbuf_info kbufInfo = device->KbufInfo();
        this->fBlockBytes = kbufInfo.block_size;
    }
    
    if (!silent) this->PrintHead(cout);
    
    this->fTimeStart.reset(new TTimer());
    
    for (int i = 0; i < this->fNRuns; i++)
    {
        this->fStartTimers[i].reset(new TTimer());
        this->fTestFn(device, this);
        this->fEndTimers[i].reset(new TTimer());
        
        if (!device->Error().empty()) break;
        
        long us = this->fEndTimers[i]->RealTime() - this->fStartTimers[i]->RealTime();
        if (this->fRunIntervalUs > us) 
        {
            usleep(this->fRunIntervalUs - us);
        }
    }
    this->fTimeEnd.reset(new TTimer());
    
    if (!silent) this->PrintSummary(cout);
}

void TTest::UpdateStatus(long newBytesRead, string error)
{
    fDoneBytes += newBytesRead;
    if (!error.empty()) fDevError.append(error); 
}

void TTest::PrintHead(ostream& file)
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

void TTest::PrintInfo(ostream& file, const char* info)
{
    file << "*** " << info << endl;
}

bool TTest::StatusOK()
{
    return (fDoneBytes == fBytesPerTest * fNRuns) && fDevError.empty();
}

void TTest::PrintSummary(ostream& file)
{
    file << endl << "**********************************************"  << endl;
    
    if (this->StatusOK())
    {
        TTimer timeTotal = *fTimeEnd   - *fTimeStart;
        file << "*** RESULT: OK!" << endl;
        file << "*** " << endl;
        file << "*** Total data size:         " << setw(10) << fixed << setprecision(0) << fBytesPerTest * fEndTimers.size() / 1024 << " kB" << endl;
        file << "*** Total clock time:        " << setw(10) << timeTotal.RealTime() << " us" << endl;
        file << "*** Total CPU time:          " << setw(10) << timeTotal.CpuTime()   << " us" << endl;  
        file << "*** Total userspace time:    " << setw(10) << timeTotal.UserTime()   << " us" << endl;  
        file << "*** Total kernel time:       " << setw(10) << timeTotal.KernelTime()   << " us" << endl;  
    }
    else
    {
        file << "*** RESULT: ERROR!" << endl;
        file << "***"                << endl;
        file << "*** Processed " << fDoneBytes << " of " << fBytesPerTest << " bytes" << endl; 
        file << "*** Device error: " << fDevError << endl;
    }
    file << "**********************************************"  << endl;
}    

void TTest::PrintStat(ostream& file, bool printHeader)
{
    if (printHeader)
    {
        file << "Average test results:"  << endl;
        cout << std::setw(fTestName.size()) << left << "Test name" << right << " | " 
        << setw(8) << "MB/s"  << " | " 
        << setw(8) << "Cpu(%)"  << " | " 
        << setw(8) << "tClk(us)" << " | " << setw(8) << "err(%)" << " | " 
        << setw(8) << "tCpu(us)" << " | " << setw(8) << "err(%)" << " | " 
        << setw(8) << "tKrn(us)" << " | " << setw(8) << "err(%)" << " | " 
        << setw(8) << "tUsr(us)" << " | " << setw(8) << "err(%)" 
        << endl;
    }
    
    if (this->StatusOK())
    {
        TTimer timeTotal = *fTimeEnd - *fTimeStart;
        
        // calculate averages
        double clkTimePerTestS = 0; 
        double cpuTimePerTestS = 0;
        double kerTimePerTestS = 0;
        double usrTimePerTestS = 0;
        
        for (size_t i = 0; i < fEndTimers.size(); i++)
        {
            TTimer timeSpent = *fEndTimers[i] - *fStartTimers[i];
            
            clkTimePerTestS += timeSpent.RealTime();
            cpuTimePerTestS += timeSpent.CpuTime();
            kerTimePerTestS += timeSpent.KernelTime();
            usrTimePerTestS += timeSpent.UserTime();
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
        
        for (size_t i = 0; i < fEndTimers.size(); i++)
        {
            TTimer timeSpent = *fEndTimers[i] - *fStartTimers[i];
            clkTimePerTestD += (timeSpent.RealTime() - clkTimePerTestS) * (timeSpent.RealTime() - clkTimePerTestS);
            cpuTimePerTestD += (timeSpent.CpuTime()  - cpuTimePerTestS) * (timeSpent.CpuTime()  - cpuTimePerTestS);
            kerTimePerTestD += (timeSpent.KernelTime() - kerTimePerTestS) * (timeSpent.KernelTime() - kerTimePerTestS);
            usrTimePerTestD += (timeSpent.UserTime() - usrTimePerTestS) * (timeSpent.UserTime() - usrTimePerTestS);
        }
        clkTimePerTestD = 100*sqrt(clkTimePerTestD)/fEndTimers.size()/clkTimePerTestS;
        cpuTimePerTestD = 100*sqrt(cpuTimePerTestD)/fEndTimers.size()/cpuTimePerTestS;
        kerTimePerTestD = 100*sqrt(kerTimePerTestD)/fEndTimers.size()/kerTimePerTestS;
        usrTimePerTestD = 100*sqrt(usrTimePerTestD)/fEndTimers.size()/usrTimePerTestS;
        
        // average cpu load
        double cpuLoad = 100.0 * timeTotal.CpuTime() / timeTotal.RealTime();
                
        file << fTestName << " | "
        << fixed << setprecision(1) << setw(8) << fBytesPerTest / clkTimePerTestS << " | " 
        << fixed << setprecision(2) << setw(8) << cpuLoad << " | "
        << fixed << setprecision(0) << setw(8) << clkTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << clkTimePerTestD << " | "
        << fixed << setprecision(0) << setw(8) << cpuTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << cpuTimePerTestD << " | "
        << fixed << setprecision(0) << setw(8) << kerTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << kerTimePerTestD << " | "
        << fixed << setprecision(0) << setw(8) << usrTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << usrTimePerTestD 
        << endl;
    }
    else
    {
        file << "Device error: " << fDevError << endl;
    }
}    
    