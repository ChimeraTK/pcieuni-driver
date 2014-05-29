#ifndef DEVTEST_TEST
#define DEVTEST_TEST

#include <vector>
#include <memory>
#include "devtest_timer.h"
#include "devtest_device.h"

using namespace std;

class TTest
{
public:
    typedef void (TFn)(IDevice* device, TTest* test);
    
    TFn*         fTestFn;
    string       fTestName;
    long         fStartOffset;
    long         fBytesPerTest;
    int          fNRuns;
    long         fRunIntervalUs;
    long         fBlockBytes;
    long         fDoneBytes;
    string       fDevError;
    unique_ptr<TTimer> fTimeStart;
    unique_ptr<TTimer> fTimeEnd;

    vector<unique_ptr<TTimer> > fStartTimers;
    vector<unique_ptr<TTimer> > fEndTimers;
    
    vector<char> fBuffer;
    
    void Init(const std::string& testName, TFn* testFn, long startOffset, long bytesPerTest, int nRuns, long runIntervalUs);
    void Run(IDevice* device, bool silent = false);
    void UpdateStatus(long newBytesRead, string error);
    void PrintHead(ostream& file);
    void PrintInfo(ostream& file, const char* info);    
    bool StatusOK();    
    void PrintSummary(ostream& file);    
    void PrintStat(ostream& file, bool printHeader = true);
};
#endif