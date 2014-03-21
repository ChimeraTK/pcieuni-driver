#include "devtest_timer.h"
#include <sys/time.h>
#include <time.h>

TTimer::TTimer()
{
//     struct timeval realTime;
//     gettimeofday(&realTime, nullptr);
//     fRealTime = realTime.tv_sec * 1000000 + realTime.tv_usec;
//     fCpuTime  = 1000000 * clock() / CLOCKS_PER_SEC;
    struct timespec tmp;
    clock_gettime(CLOCK_REALTIME, &tmp);
    fRealTime = tmp.tv_sec * 1000000 + tmp.tv_nsec / 1000;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tmp);
    fCpuTime = tmp.tv_sec * 1000000 + tmp.tv_nsec / 1000;
}

TTimer TTimer::operator-(const TTimer& other)
{
    TTimer result;
    result.fCpuTime  = this->fCpuTime  - other.fCpuTime;
    result.fRealTime = this->fRealTime - other.fRealTime;
    
    return result;
}
