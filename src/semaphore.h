/**
    @file
    @author Mark Boady <mwb33@drexel.edu>
    @date 2024
    @section Description

    This class implements a classic Dijskra semaphore using condition variables and a mutex lock.
 */
#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <mutex>
#include <condition_variable>

/**
    A semaphore class based on Dijsktra's design, but implemented using C++ 17 mutex and condition variable. 
 */
class Semaphore{
private:
    mutable std::mutex counterLock;/**< A lock is required to protect the counter. Mutable allows the lock to be changed in a const function call. */
    std::condition_variable cv;/**< The condition variable is used to sleep until the counter is great than zero. */
    unsigned int counter;/**< The counter to determine if the thread waits or not. */
public:
    /**
        Create a new Semaphore. The counter defaults to 1.
     */
    Semaphore();
    /**
        Create a Semaphore with counter at a custom value.
        @param start is the value to start the counter at.
     */
    Semaphore(unsigned int start);
    /**
        Increase the counter by 1. If any threads are waiting, wake one up.
     */
    void signal();
    /**
        Decrease the counter by 1. If the counter is zero, sleep until a signal.
     */
    void wait();
    /**
        Print the current status of the counter (for debugging).
        The friend command means this function can see the private attributes.
        The const command means this function will not change the object. (Look but don't touch.)
        @param os is the stream to print to.
        @param s is the semaphore to print
        @return the stream after changes (so you can do `cout << b << a << c;`)
     */
    friend std::ostream& operator<<(std::ostream& os, const Semaphore& s);
};

#endif