/**
 *  @file   devtest_test.cpp
 *  @brief  Declaration of test classes
 *
 *  Test classes run operations on device and collect test results           
 */

#ifndef DEVTEST_TEST

#define DEVTEST_TEST

#include <vector>
#include <memory>
#include "devtest_timer.h"
#include "devtest_device.h"

using namespace std;

class TTest;
class TDevTest;


/**
 * @brief Executes test operation on device
 * 
 * @param device    Device to be tested 
 * @param test      Test worker/reporter class
 */
typedef void (TFn)(IDevice* device, TDevTest* test);

/**
 * @brief Implements main test worker and reporter class
 * 
 * TTest runs testing procedure on series of target devices and collects performance results.
 * It also provides functions to print the results on screen.
 */

class TTest
{
public:
    TFn*         fTestFn;
    string       fTestName;
    long         fStartOffset;
    long         fBytesPerTest;
    int          fNRuns;
    long         fRunIntervalUs;
    unique_ptr<TTimer> fTimeStart;
    unique_ptr<TTimer> fTimeEnd;
    vector<unique_ptr<TTimer> > fStartTimers;
    vector<unique_ptr<TTimer> > fEndTimers;
    vector<unique_ptr<TDevTest> > fDevTests;
    
    void Init(const std::string& testName, TFn* testFn, long startOffset, long bytesPerTest, int nRuns, long runIntervalUs);
    void Run(vector<shared_ptr<IDevice> > &devices, bool silent = false);
    vector<char>& Buffer(int devTest);
    void PrintHead(ostream& file);
    bool StatusOK();    
    void PrintSummary(ostream& file);    
    void PrintStat(ostream& file, bool printHeader = true);
};

class TDevTest
{
    
public:
    string       fTestName;
    IDevice*     fDevice;
    TFn*         fTestFn;
    int          fNRuns;
    
    vector<unique_ptr<TTimer> > fStartTimers;
    vector<unique_ptr<TTimer> > fEndTimers;
    vector<char> fBuffer;
    long         fStartOffset;
    long         fBytesPerTest;
    long         fDoneBytes;
    string       fDevError;
    
    TDevTest(string testName, IDevice* device, TFn* testFn, long startOffset, long bytesPerTest, int nRuns);
    bool Run(int testIndex);
    void UpdateStatus(long newBytesRead, string error);
    vector<char>& Buffer();
    IDevice* Device() const;
    
    void PrintSummary(ostream& file) const;
    void PrintStat(ostream& file) const;
    bool StatusOK() const;
};

#endif