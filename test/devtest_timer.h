#ifndef TTIMER_H
#define TTIMER_H


class TTimer
{
public:
    TTimer();
    
    long RealTime() { return fRealTime; }
    long CpuTime() { return fCpuTime; }
    long UserTime() { return fUserTime; }
    long KernelTime() { return fKernelTime; }
    
    TTimer operator-(const TTimer& other);
private:
    long fRealTime;
    long fCpuTime;
    long fUserTime;
    long fKernelTime;
    
};

#endif // TTIMER_H
