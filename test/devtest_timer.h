/**
 *  @file   devtest_timer.h
 *  @brief  Declaration of TTimer class 
 */

#ifndef TTIMER_H
#define TTIMER_H


/**
 * @brief Implements an accurate timestamp with several different timers  
 * 
 */
class TTimer
{
public:
    TTimer();
    
    /** @brief Wall clock timestamp
     *  @return long int
     */
    long RealTime() { return fRealTime; }

    /** @brief Cpu tick timestamp
     *  @return long int
     */
    long CpuTime() { return fCpuTime; }

    /** @brief Process user-space tick timestamp
     *  @return long int
     */
    long UserTime() { return fUserTime; }
    
    /** @brief Process kernel-space tick timestamp
     *  @return long int
     */
    long KernelTime() { return fKernelTime; }
    
    TTimer operator-(const TTimer& other);
private:
    long fRealTime;     /**< Wall clock timestamp  */
    long fCpuTime;      /**< Cpu tick timestamp */
    long fUserTime;     /**< Process user-space tick timestamp */
    long fKernelTime;   /**< Process kernel-space tick timestamp */
};

#endif // TTIMER_H
