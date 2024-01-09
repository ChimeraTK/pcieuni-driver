/**
 *  @file   devtest_test.h
 *  @brief  Declaration of test classes
 *
 *  Test classes run operations on device and collect test results
 */

#ifndef DEVTEST_TEST

#define DEVTEST_TEST

#include "devtest_device.h"
#include "devtest_timer.h"

#include <memory>
#include <vector>

using namespace std;

class TTest;
class TDevTest;

/**
 * @brief Executes test operation on device
 *
 * @param device    Device to be tested
 * @param test      Test worker/reporter class
 */
typedef void(TFn)(IDevice* device, TDevTest* test);

/**
 * @brief Implements main test worker and reporter class
 *
 * TTest runs testing procedure on series of target devices and collects performance results.
 * Consumer can choose how many times the test should be run. The final results are averages
 * of multiple test runs.
 */
class TTest {
  TFn* fTestFn;                            /**< Test operation to be executed */
  string fTestName;                        /**< Test name as is displayed in UI */
  long fStartOffset;                       /**< DMA offset to read from */
  long fBytesPerTest;                      /**< Number of bytes to transfer per read */
  int fNRuns;                              /**< Number of test runs */
  long fRunIntervalUs;                     /**< Time interval between consecutive test runs  */
  unique_ptr<TTimer> fTimeStart;           /**< Testing start timestamp */
  unique_ptr<TTimer> fTimeEnd;             /**< Testing end timestamp */
  vector<unique_ptr<TTimer>> fStartTimers; /**< List of test-start timestamps (one per each test run, wehere test run
                                              includes all the target devices) */
  vector<unique_ptr<TTimer>> fEndTimers;   /**< List of test-end timestamps (one per each test run, wehere test run
                                              includes all the target devices) */
  vector<unique_ptr<TDevTest>> fDevTests;  /**< List of per-device test workers */

 public:
  void Init(
      const std::string& testName, TFn* testFn, long startOffset, long bytesPerTest, int nRuns, long runIntervalUs);
  void Run(vector<shared_ptr<IDevice>>& devices, bool silent = false);
  vector<char>& Buffer(int devTest);
  void PrintHead(ostream& file);
  bool StatusOK();
  void PrintSummary(ostream& file);
  void PrintStat(ostream& file, bool printHeader = true);
};

/**
 * @brief Implements per-device test worker class
 *
 * TDevTest is used to repeatedly run test procedure on a single target device and log the
 * test results of each run.
 */
class TDevTest {
 public:
  string fTestName; /**< Test name as is displayed in UI */
  IDevice* fDevice; /**< Target device */
  TFn* fTestFn;     /**< Test operation to be executed */
  int fNRuns;       /**< Number of test runs */

  vector<unique_ptr<TTimer>> fStartTimers; /**< List of test-start timestamps (one per each test run) */
  vector<unique_ptr<TTimer>> fEndTimers;   /**< List of test-start timestamps (one per each test run) */
  vector<char> fBuffer;                    /**< DMA data is copied into this buffer */
  long fStartOffset;                       /**< DMA offset to read from */
  long fBytesPerTest;                      /**< Number of bytes to transfer per read */
  long fDoneBytes;                         /**< Total number of bytes read from target device */
  string fDevError;                        /**< Error description; empty if there was no error */

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