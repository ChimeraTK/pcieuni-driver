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
    fStartTimers.resize(nRuns);
    fEndTimers.resize(nRuns);
}

void TTest::Run(vector<unique_ptr<IDevice> > &devices, bool silent)
{
    // Prepare list of per-device tests
    fDevTests.resize(devices.size());
    for (unsigned int i = 0; i < devices.size(); i++)
    {
        fDevTests[i].reset(new TDevTest(fTestName, devices[i].get(), fTestFn, fStartOffset, fBytesPerTest, fNRuns));
    }

    // print test header
    if (!silent) this->PrintHead(cout);

    // Take testing-start timestamp
    this->fTimeStart.reset(new TTimer());

    // Run test fNRuns times
    for (int i = 0; i < this->fNRuns; i++)
    {
        
        // Take test-start timestamp
        fStartTimers[i].reset(new TTimer());
        
        // Run test on each device
        for (unsigned int d = 0; d < fDevTests.size(); d++)
        {
            fDevTests[d].get()->Run(i);
        }
        fEndTimers[i].reset(new TTimer());
        // Take test-end timestamp
        
        // sleep until it is time for the next test run
        long us = fEndTimers[i].get()->RealTime() - fStartTimers[i].get()->RealTime();
        if (this->fRunIntervalUs > us) 
        {
            usleep(this->fRunIntervalUs - us);
        }
    }

    // Take testing-end timestamp    
    this->fTimeEnd.reset(new TTimer());
    
    // Print test results
    if (!silent) this->PrintSummary(cout);
}

vector<char>& TTest::Buffer(int devTest)
{
    return fDevTests[devTest]->Buffer();
}


void TTest::PrintHead(ostream& file)
{
    file << endl << endl << endl;
    file << "**********************************************" << endl;
    file << "*** TEST: " << fTestName << endl;
    file << "***"                     << endl;
    file << "*** DMA offset         : " << hex << fStartOffset << dec << endl;
    file << "*** transfer size (kB) : " << fBytesPerTest/1024 << endl;
    file << "*** number of test runs: " << fNRuns << endl;
    file << "*** Target devices: " << endl;
    for (std::vector<unique_ptr<TDevTest> >::iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end(); ++iDevTest)
    {
        file << "***       " << iDevTest->get()->Device()->Name() << endl;
    }
    file << "**********************************************" << endl;
}

bool TTest::StatusOK()
{
    bool statusOK(true);
    
    for (vector<unique_ptr<TDevTest> >::const_iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end(); iDevTest++)
    {
        statusOK &= iDevTest->get()->StatusOK();
    }
    
    return statusOK;
}

// TODO: handle case of only one device
void TTest::PrintSummary(ostream& file)
{
    file << endl; 
    file << "**********************************************"  << endl;
    file << "*** TEST RESULTS"  << endl;
    file << "**********************************************"  << endl;
    
    if (this->StatusOK())
    {
        TTimer timeTotal = *fTimeEnd   - *fTimeStart;
        file << "*** RESULT: OK!" << endl;
        file << "*** " << endl;
        file << "*** Total data size:         " << setw(10) << fixed << setprecision(0) << fDevTests.size() * fEndTimers.size() * fBytesPerTest / 1024 << " kB" << endl;
        file << "*** Total clock time:        " << setw(10) << timeTotal.RealTime() << " us" << endl;
        file << "*** Total CPU time:          " << setw(10) << timeTotal.CpuTime()   << " us" << endl;  
        file << "*** Total userspace time:    " << setw(10) << timeTotal.UserTime()   << " us" << endl;  
        file << "*** Total kernel time:       " << setw(10) << timeTotal.KernelTime()   << " us" << endl;  
    }
    else
    {
        file << "*** RESULT: ERROR!" << endl;
    }
    file << endl; 
    file << "**********************************************"  << endl;
    file << "*** STATUS PER DEVICE:"  << endl;
    file << "**********************************************"  << endl;
    
    for (vector<unique_ptr<TDevTest> >::const_iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end(); iDevTest++)
    {
        iDevTest->get()->PrintSummary(file);
    }
}    

// TODO: handle case of only one device
void TTest::PrintStat(ostream& file, bool printHeader)
{
    if (printHeader)
    {
        file << "Average test results:"  << endl;
        cout << std::setw(fTestName.size()) << left << "Test name" << right << " | "
        << std::setw(15) << left << "Device " << right << " | "
        << setw(8) << "MB/s"  << " | " 
        << setw(8) << "Cpu(%)"  << " | " 
        << setw(8) << "tClk(us)" << " | " << setw(8) << "err(%)" << " | " 
        << setw(8) << "tCpu(us)" << " | " << setw(8) << "err(%)" << " | " 
        << setw(8) << "tKrn(us)" << " | " << setw(8) << "err(%)" << " | " 
        << setw(8) << "tUsr(us)" << " | " << setw(8) << "err(%)" 
        << endl;
    }

    for (vector<unique_ptr<TDevTest> >::const_iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end(); iDevTest++)
    {
        iDevTest->get()->PrintStat(file);
    }   
    
    if (this->StatusOK())
    {
        TTimer timeTotal = *fTimeEnd - *fTimeStart;
        
        // calculate averages
        double clkTimePerTestS = 0; 
        double cpuTimePerTestS = 0;
        double kerTimePerTestS = 0;
        double usrTimePerTestS = 0;
        
        for (int i = 0; i < fNRuns; i++)
        {
            TTimer timeSpent = *fEndTimers[i] - *fStartTimers[i];
            
            clkTimePerTestS += timeSpent.RealTime();
            cpuTimePerTestS += timeSpent.CpuTime();
            kerTimePerTestS += timeSpent.KernelTime();
            usrTimePerTestS += timeSpent.UserTime();
        }
        clkTimePerTestS = clkTimePerTestS / fNRuns;
        cpuTimePerTestS = cpuTimePerTestS / fNRuns;
        kerTimePerTestS = kerTimePerTestS / fNRuns;
        usrTimePerTestS = usrTimePerTestS / fNRuns;
        
        // calculate deviations
        double clkTimePerTestD = 0; 
        double cpuTimePerTestD = 0;
        double kerTimePerTestD = 0;
        double usrTimePerTestD = 0;
        
        for (int i = 0; i < fNRuns; i++)
        {
            TTimer timeSpent = *fEndTimers[i] - *fStartTimers[i];
            clkTimePerTestD += (timeSpent.RealTime() - clkTimePerTestS) * (timeSpent.RealTime() - clkTimePerTestS);
            cpuTimePerTestD += (timeSpent.CpuTime()  - cpuTimePerTestS) * (timeSpent.CpuTime()  - cpuTimePerTestS);
            kerTimePerTestD += (timeSpent.KernelTime() - kerTimePerTestS) * (timeSpent.KernelTime() - kerTimePerTestS);
            usrTimePerTestD += (timeSpent.UserTime() - usrTimePerTestS) * (timeSpent.UserTime() - usrTimePerTestS);
        }
        clkTimePerTestD = 100*sqrt(clkTimePerTestD)/fNRuns/clkTimePerTestS;
        cpuTimePerTestD = 100*sqrt(cpuTimePerTestD)/fNRuns/cpuTimePerTestS;
        kerTimePerTestD = 100*sqrt(kerTimePerTestD)/fNRuns/kerTimePerTestS;
        usrTimePerTestD = 100*sqrt(usrTimePerTestD)/fNRuns/usrTimePerTestS;
        
        // average cpu load
        double cpuLoad = 100.0 * timeTotal.CpuTime() / timeTotal.RealTime();
                
        file << fTestName << " | "
        << left << std::setw(15) << "SUM " << right << " | "
        << fixed << setprecision(1) << setw(8) << fBytesPerTest * fDevTests.size() / clkTimePerTestS << " | " 
        << fixed << setprecision(2) << setw(8) << cpuLoad << " | "
        << fixed << setprecision(0) << setw(8) << clkTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << clkTimePerTestD << " | "
        << fixed << setprecision(0) << setw(8) << cpuTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << cpuTimePerTestD << " | "
        << fixed << setprecision(0) << setw(8) << kerTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << kerTimePerTestD << " | "
        << fixed << setprecision(0) << setw(8) << usrTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << usrTimePerTestD 
        << endl;
    }
    else
    {
        file << "ERROR"<< endl;
    }
    
}    

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

TDevTest::TDevTest(string testName, IDevice* device, TFn* testFn, long startOffset, long bytesPerTest, int nRuns)
{
    fTestName = testName;
    fDevice = device;
    fTestFn = testFn;
    fStartOffset = startOffset;
    fBytesPerTest = bytesPerTest;
    fBuffer.resize(bytesPerTest);
    fNRuns = nRuns;
    fStartTimers.resize(nRuns);
    fEndTimers.resize(nRuns);
    fDoneBytes = 0;
    
    fill(fBuffer.begin(),fBuffer.end(), 0x42);
    device->ResetStatus();
}

bool TDevTest::Run(int testIndex)
{
    this->fStartTimers[testIndex].reset(new TTimer());
    this->fTestFn(fDevice, this);
    this->fEndTimers[testIndex].reset(new TTimer());
    
    return fDevice->Error().empty();
}


vector<char>& TDevTest::Buffer()
{
    return fBuffer;
}

IDevice* TDevTest::Device() const
{
    return fDevice;
}

void TDevTest::PrintSummary(ostream& file) const
{
    file << "*** Device "  << fDevice->Name() << endl;
    file << "**********************************************"  << endl;
    
    if (this->StatusOK())
    {
        file << "*** RESULT: OK!" << endl;
        file << "*** " << endl;
        file << "*** Total data size:         " << setw(10) << fixed << setprecision(0) << fBytesPerTest * fEndTimers.size() / 1024 << " kB" << endl;
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

void TDevTest::PrintStat(ostream& file) const
{
    if (this->StatusOK())
    {
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
        
        file << fTestName << " | "
             << left << std::setw(15) << fDevice->Name() << right << " | "
             << fixed << setprecision(1) << setw(8) << fBytesPerTest / clkTimePerTestS << " | " 
             << fixed << setprecision(2) << setw(8) << " " << " | "
             << fixed << setprecision(0) << setw(8) << clkTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << clkTimePerTestD << " | "
             << fixed << setprecision(0) << setw(8) << cpuTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << cpuTimePerTestD << " | "
             << fixed << setprecision(0) << setw(8) << kerTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << kerTimePerTestD << " | "
             << fixed << setprecision(0) << setw(8) << usrTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << usrTimePerTestD 
             << endl;
    }
    else
    {
        file << fTestName << " | "
             << std::setw(15) << fDevice->Name() << " | "
             "Device error: " << fDevError << endl;
    }
}    

void TDevTest::UpdateStatus(long newBytesRead, string error)
{
    fDoneBytes += newBytesRead;
    if (!error.empty()) fDevError.append(error); 
}

bool TDevTest::StatusOK() const
{
    return (fDoneBytes == fBytesPerTest * fNRuns) && fDevError.empty();
}
