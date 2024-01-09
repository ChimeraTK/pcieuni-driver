/**
 *  @file   devtest_test.cpp
 *  @brief  Implementation of test classes
 *
 *  Test classes run operations on device and collect test results
 */

#include "devtest_test.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>

/**
 * @brief Initialize before test-run.
 *
 * @param testName      Test name as is displayed in UI
 * @param testFn        Test operation to be executed
 * @param startOffset   DMA offset to read from
 * @param bytesPerTest  Number of bytes to transfer per read
 * @param nRuns         Number of test runs
 * @param runIntervalUs ime interval between consecutive test runs (in microseconds)
 * @return void
 */
void TTest::Init(
    const std::string& testName, TFn* testFn, long startOffset, long bytesPerTest, int nRuns, long runIntervalUs) {
  fTestName = testName;
  fTestFn = testFn;
  fStartOffset = startOffset;
  fBytesPerTest = bytesPerTest;
  fNRuns = nRuns;
  fRunIntervalUs = runIntervalUs;
  fStartTimers.resize(nRuns);
  fEndTimers.resize(nRuns);
}

/**
 * @brief Run the test
 *
 * @param devices   List of devices to run the test on
 * @param silent    When true test headers and results will not be printed - useful to produce single output for many tests
 * @return void
 */
void TTest::Run(vector<shared_ptr<IDevice>>& devices, bool silent) {
  // Prepare list of per-device tests
  fDevTests.resize(devices.size());
  for(unsigned int i = 0; i < devices.size(); i++) {
    fDevTests[i].reset(new TDevTest(fTestName, devices[i].get(), fTestFn, fStartOffset, fBytesPerTest, fNRuns));
  }

  // print test header
  if(!silent) this->PrintHead(cout);

  // Take testing-start timestamp
  this->fTimeStart.reset(new TTimer());

  // Run test fNRuns times
  for(int i = 0; i < this->fNRuns; i++) {
    bool workDone = false;
    // Take test-start timestamp
    fStartTimers[i].reset(new TTimer());

    // Run test on each device
    for(unsigned int d = 0; d < fDevTests.size(); d++) {
      workDone |= fDevTests[d].get()->Run(i);
    }
    fEndTimers[i].reset(new TTimer());
    // Take test-end timestamp

    if(!workDone) {
      // no work was done, all devices must be in error state
      break;
    }

    // sleep until it is time for the next test run
    long us = fEndTimers[i].get()->RealTime() - fStartTimers[i].get()->RealTime();
    if(this->fRunIntervalUs > us) {
      usleep(this->fRunIntervalUs - us);
    }

    if(!silent && (i >= 500)) {
      if((i % 500) == 0) {
        cout << fixed << setprecision(2) << "Done: " << (100.0 * i) / this->fNRuns << " %" << endl;
      }
    }
  }

  // Take testing-end timestamp
  this->fTimeEnd.reset(new TTimer());

  // Print test results
  if(!silent) this->PrintSummary(cout);
}

/**
 * @brief Returns target buffer that data is read into
 *
 * @param devTest   Return buffer for this device (index)
 * @return  The buffer
 */
vector<char>& TTest::Buffer(int devTest) {
  return fDevTests[devTest]->Buffer();
}

/**
 * @brief   Print test header
 *
 * @param   Target stream
 * @return void
 */
void TTest::PrintHead(ostream& file) {
  file << endl << endl << endl;
  file << "**********************************************" << endl;
  file << "*** TEST: " << fTestName << endl;
  file << "***" << endl;
  file << "*** DMA offset         : " << hex << fStartOffset << dec << endl;
  file << "*** transfer size (kB) : " << fBytesPerTest / 1024 << endl;
  file << "*** number of test runs: " << fNRuns << endl;
  file << "*** Target devices: " << endl;
  for(std::vector<unique_ptr<TDevTest>>::iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end();
      ++iDevTest) {
    file << "***       " << iDevTest->get()->Device()->Name() << endl;
  }
  file << "**********************************************" << endl;
}

/**
 * @brief Returns true if no error occured during testing on any device
 *
 * @return bool
 */
bool TTest::StatusOK() {
  bool statusOK(true);

  for(vector<unique_ptr<TDevTest>>::const_iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end();
      iDevTest++) {
    statusOK &= iDevTest->get()->StatusOK();
  }

  return statusOK;
}

/**
 * @brief Print test results summary
 *
 * @param file Target stream
 * @return void
 */
void TTest::PrintSummary(ostream& file) {
  file << endl;
  file << "**********************************************" << endl;
  file << "*** TEST RESULTS" << endl;
  file << "**********************************************" << endl;

  if(this->StatusOK()) {
    TTimer timeTotal = *fTimeEnd - *fTimeStart;
    file << "*** RESULT: OK!" << endl;
    file << "*** " << endl;
    file << "*** Total data size:         " << setw(10) << fixed << setprecision(0)
         << fDevTests.size() * fEndTimers.size() * fBytesPerTest / 1024 << " kB" << endl;
    file << "*** Total clock time:        " << setw(10) << timeTotal.RealTime() << " us" << endl;
    file << "*** Total CPU time:          " << setw(10) << timeTotal.CpuTime() << " us" << endl;
    file << "*** Total userspace time:    " << setw(10) << timeTotal.UserTime() << " us" << endl;
    file << "*** Total kernel time:       " << setw(10) << timeTotal.KernelTime() << " us" << endl;
  }
  else {
    file << "*** RESULT: ERROR!" << endl;
  }
  file << endl;
  file << "**********************************************" << endl;
  file << "*** STATUS PER DEVICE:" << endl;
  file << "**********************************************" << endl;

  for(vector<unique_ptr<TDevTest>>::const_iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end();
      iDevTest++) {
    iDevTest->get()->PrintSummary(file);
  }
}

/**
 * @brief Print test performance results, averages and error estimation
 *
 * @param file          Target stream
 * @param printHeader   Set to false to skip header line - useful to print results of multiple tests into single table
 * @return void
 */
void TTest::PrintStat(ostream& file, bool printHeader) {
  if(printHeader) {
    file << "Average test results:" << endl;
    cout << std::setw(fTestName.size()) << left << "Test name" << right << " | " << std::setw(15) << left << "Device "
         << right << " | " << setw(8) << "MB/s"
         << " | " << setw(8) << "Cpu(%)"
         << " | " << setw(8) << "tClk(us)"
         << " | " << setw(8) << "err(%)"
         << " | " << setw(8) << "tCpu(us)"
         << " | " << setw(8) << "err(%)"
         << " | " << setw(8) << "tKrn(us)"
         << " | " << setw(8) << "err(%)"
         << " | " << setw(8) << "tUsr(us)"
         << " | " << setw(8) << "err(%)" << endl;
  }

  for(vector<unique_ptr<TDevTest>>::const_iterator iDevTest = fDevTests.begin(); iDevTest != fDevTests.end();
      iDevTest++) {
    iDevTest->get()->PrintStat(file);
  }

  if(this->StatusOK()) {
    TTimer timeTotal = *fTimeEnd - *fTimeStart;

    // calculate averages
    double clkTimePerTestS = 0;
    double cpuTimePerTestS = 0;
    double kerTimePerTestS = 0;
    double usrTimePerTestS = 0;

    for(int i = 0; i < fNRuns; i++) {
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

    for(int i = 0; i < fNRuns; i++) {
      TTimer timeSpent = *fEndTimers[i] - *fStartTimers[i];
      clkTimePerTestD += (timeSpent.RealTime() - clkTimePerTestS) * (timeSpent.RealTime() - clkTimePerTestS);
      cpuTimePerTestD += (timeSpent.CpuTime() - cpuTimePerTestS) * (timeSpent.CpuTime() - cpuTimePerTestS);
      kerTimePerTestD += (timeSpent.KernelTime() - kerTimePerTestS) * (timeSpent.KernelTime() - kerTimePerTestS);
      usrTimePerTestD += (timeSpent.UserTime() - usrTimePerTestS) * (timeSpent.UserTime() - usrTimePerTestS);
    }
    clkTimePerTestD = 100 * sqrt(clkTimePerTestD) / fNRuns / clkTimePerTestS;
    cpuTimePerTestD = 100 * sqrt(cpuTimePerTestD) / fNRuns / cpuTimePerTestS;
    kerTimePerTestD = 100 * sqrt(kerTimePerTestD) / fNRuns / kerTimePerTestS;
    usrTimePerTestD = 100 * sqrt(usrTimePerTestD) / fNRuns / usrTimePerTestS;

    // average cpu load
    double cpuLoad = 100.0 * timeTotal.CpuTime() / timeTotal.RealTime();

    file << fTestName << " | " << left << std::setw(15) << "SUM " << right << " | " << fixed << setprecision(1)
         << setw(8) << fBytesPerTest * fDevTests.size() / clkTimePerTestS << " | " << fixed << setprecision(2)
         << setw(8) << cpuLoad << " | " << fixed << setprecision(0) << setw(8) << clkTimePerTestS << " | " << fixed
         << setprecision(1) << setw(8) << clkTimePerTestD << " | " << fixed << setprecision(0) << setw(8)
         << cpuTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << cpuTimePerTestD << " | " << fixed
         << setprecision(0) << setw(8) << kerTimePerTestS << " | " << fixed << setprecision(1) << setw(8)
         << kerTimePerTestD << " | " << fixed << setprecision(0) << setw(8) << usrTimePerTestS << " | " << fixed
         << setprecision(1) << setw(8) << usrTimePerTestD << endl;
  }
  else {
    file << "ERROR" << endl;
  }
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

/**
 * @brief Constructor
 *
 * @param testName      Test name as is displayed in UI
 * @param device        Target device
 * @param testFn        Test operation to be executed
 * @param startOffset   DMA offset to read from
 * @param bytesPerTest  Number of bytes to transfer per read
 * @param nRuns         Number of test runs
 */
TDevTest::TDevTest(string testName, IDevice* device, TFn* testFn, long startOffset, long bytesPerTest, int nRuns) {
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

  fill(fBuffer.begin(), fBuffer.end(), 0x42);
  device->ResetStatus();
}

/**
 * @brief Run the test
 *
 * @param testIndex     Index of test run
 * @retval true     Test run OK
 * @retval false    Test failed
 */
bool TDevTest::Run(int testIndex) {
  if(fDevice->Error().empty()) {
    this->fStartTimers[testIndex].reset(new TTimer());
    this->fTestFn(fDevice, this);
    this->fEndTimers[testIndex].reset(new TTimer());
  }

  return fDevice->Error().empty();
}

/**
 * @brief Target buffer that data is read into
 *
 * @return The buffer
 */
vector<char>& TDevTest::Buffer() {
  return fBuffer;
}

/**
 * @brief Target device
 *
 * @return IDevice*
 */
IDevice* TDevTest::Device() const {
  return fDevice;
}

/**
 * @brief Print test results summary for single device
 *
 * @param file Target stream
 * @return void
 */
void TDevTest::PrintSummary(ostream& file) const {
  file << "*** Device " << fDevice->Name() << endl;
  file << "**********************************************" << endl;

  if(this->StatusOK()) {
    file << "*** RESULT: OK!" << endl;
    file << "*** " << endl;
    file << "*** Total data size:         " << setw(10) << fixed << setprecision(0) << fBytesPerTest * fNRuns / 1024
         << " kB" << endl;
  }
  else {
    file << "*** RESULT: ERROR!" << endl;
    file << "***" << endl;
    file << "*** Processed " << fDoneBytes << " of " << fBytesPerTest * fNRuns << " bytes" << endl;
    file << "*** Device error: " << fDevError << endl;
  }
  file << "**********************************************" << endl;
}

/**
 * @brief Print test performance results, averages and error estimation for single device
 *
 * @param file          Target stream
 * @return void
 */
void TDevTest::PrintStat(ostream& file) const {
  if(this->StatusOK()) {
    // calculate averages
    double clkTimePerTestS = 0;
    double cpuTimePerTestS = 0;
    double kerTimePerTestS = 0;
    double usrTimePerTestS = 0;

    for(size_t i = 0; i < fEndTimers.size(); i++) {
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

    for(size_t i = 0; i < fEndTimers.size(); i++) {
      TTimer timeSpent = *fEndTimers[i] - *fStartTimers[i];
      clkTimePerTestD += (timeSpent.RealTime() - clkTimePerTestS) * (timeSpent.RealTime() - clkTimePerTestS);
      cpuTimePerTestD += (timeSpent.CpuTime() - cpuTimePerTestS) * (timeSpent.CpuTime() - cpuTimePerTestS);
      kerTimePerTestD += (timeSpent.KernelTime() - kerTimePerTestS) * (timeSpent.KernelTime() - kerTimePerTestS);
      usrTimePerTestD += (timeSpent.UserTime() - usrTimePerTestS) * (timeSpent.UserTime() - usrTimePerTestS);
    }
    clkTimePerTestD = 100 * sqrt(clkTimePerTestD) / fEndTimers.size() / clkTimePerTestS;
    cpuTimePerTestD = 100 * sqrt(cpuTimePerTestD) / fEndTimers.size() / cpuTimePerTestS;
    kerTimePerTestD = 100 * sqrt(kerTimePerTestD) / fEndTimers.size() / kerTimePerTestS;
    usrTimePerTestD = 100 * sqrt(usrTimePerTestD) / fEndTimers.size() / usrTimePerTestS;

    file << fTestName << " | " << left << std::setw(15) << fDevice->Name() << right << " | " << fixed << setprecision(1)
         << setw(8) << fBytesPerTest / clkTimePerTestS << " | " << fixed << setprecision(2) << setw(8) << " "
         << " | " << fixed << setprecision(0) << setw(8) << clkTimePerTestS << " | " << fixed << setprecision(1)
         << setw(8) << clkTimePerTestD << " | " << fixed << setprecision(0) << setw(8) << cpuTimePerTestS << " | "
         << fixed << setprecision(1) << setw(8) << cpuTimePerTestD << " | " << fixed << setprecision(0) << setw(8)
         << kerTimePerTestS << " | " << fixed << setprecision(1) << setw(8) << kerTimePerTestD << " | " << fixed
         << setprecision(0) << setw(8) << usrTimePerTestS << " | " << fixed << setprecision(1) << setw(8)
         << usrTimePerTestD << endl;
  }
  else {
    file << fTestName << " | " << std::setw(15) << fDevice->Name()
         << " | "
            "Device error: "
         << fDevError << endl;
  }
}

/**
 * @brief Update test status after test procedure run
 *
 * @param newBytesRead      How many bytes was just read
 * @param error             Which error is reported on device
 * @return void
 */
void TDevTest::UpdateStatus(long newBytesRead, string error) {
  fDoneBytes += newBytesRead;
  if(!error.empty()) fDevError.append(error);
}

/**
 * @brief Returns true if no errors happened on the target device
 *
 * @return bool
 */
bool TDevTest::StatusOK() const {
  return (fDoneBytes == fBytesPerTest * fNRuns) && fDevError.empty();
}
