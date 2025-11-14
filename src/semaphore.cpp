/**
    @file
    @author Mark Boady <mwb33@drexel.edu>
    @date 2024
    @section Description

    This file gives the implementation details of the semaphore class.
 */

 #include "semaphore.h"
 #include <sstream>
 #include <string>

 Semaphore::Semaphore(){
    counter = 1;
 }

 Semaphore::Semaphore(unsigned int start){
    counter = start;
 }

 void Semaphore::signal(){
    {std::lock_guard<std::mutex> guard(counterLock);
        counter++;
    }//End of scope for the lock guard
    cv.notify_all();//Tell all threads to check if it is their turn.
 }

 void Semaphore::wait(){
    //Lock to protect the counter
    std::unique_lock<std::mutex> guard(counterLock);
    //Wait until the counter is large enough was can decrement
    cv.wait(guard, [this]{return counter > 0;});
    counter--;
 }

std::ostream& operator<<(std::ostream& os, const Semaphore& s){
    //Use streams to make a string
    std::ostringstream toOutput;
    //Lock read to the counter
    {std::lock_guard<std::mutex> guard(s.counterLock);
         toOutput << "[Semaphore Counter: " << s.counter << "]";
    }
    std::string str = toOutput.str();
    //print the string all at once (to avoid interleaving)
    os << str;
    return os;
 }
