#ifndef TTIMER_H
#define TTIMER_H


class TTimer
{
public:
    TTimer();
    
    long RealTime() { return fRealTime; }
    long CpuTime() { return fCpuTime; }
    
    TTimer operator-(const TTimer& other);
private:
    long fRealTime;
    long fCpuTime;
    
};

#endif // TTIMER_H
