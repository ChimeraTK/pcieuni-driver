/**
 *  @file   devtest_timer.cpp
 *  @brief  Implementation of TTimer class 
 */

#include "devtest_timer.h"
#include <sys/time.h>
#include <time.h>
#include <sys/resource.h>

/**
 * @brief Constructor - automatically takes timestamp at creation time. 
 */
TTimer::TTimer()
{
    struct timespec tmp;
    clock_gettime(CLOCK_REALTIME, &tmp);
    fRealTime = tmp.tv_sec * 1000000 + tmp.tv_nsec / 1000;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tmp);
    fCpuTime = tmp.tv_sec * 1000000 + tmp.tv_nsec / 1000;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    
    fUserTime   = usage.ru_utime.tv_sec * 1000000 + usage.ru_utime.tv_usec;
    fKernelTime = usage.ru_stime.tv_sec * 1000000 + usage.ru_stime.tv_usec;
    
}

/**
 * @brief Subtracts two timestamps - the resulting timestamp contains time difference
 * 
 * @param other     The other timestamp (should be earlier than this one)
 * @return          Time difference between the two timestamps
 */
TTimer TTimer::operator-(const TTimer& other)
{
    TTimer result;
    result.fCpuTime  = this->fCpuTime  - other.fCpuTime;
    result.fRealTime = this->fRealTime - other.fRealTime;
    result.fUserTime  = this->fUserTime  - other.fUserTime;
    result.fKernelTime = this->fKernelTime - other.fKernelTime;
    
    return result;
}
