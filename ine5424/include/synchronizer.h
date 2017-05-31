// EPOS Synchronizer Abstractions Common Package

#ifndef __synchronizer_h
#define __synchronizer_h

#include <cpu.h>
#include <thread.h>

__BEGIN_SYS

class Synchronizer_Common
{
protected:
    Synchronizer_Common() {}

    // Atomic operations
    bool tsl(volatile bool & lock) { return CPU::tsl(lock); }
    int finc(volatile int & number) { return CPU::finc(number); }
    int fdec(volatile int & number) { return CPU::fdec(number); }

    // Thread operations
    void begin_atomic() { Thread::lock(); }
    void end_atomic() { Thread::unlock(); }

    void sleep() { Thread::sleep(&sleeping); } // implicit unlock()
    void wakeup() { Thread::wakeup(&sleeping); }
    void wakeup_all() { Thread::wakeup_all(&sleeping); }

protected:
    Thread::Queue sleeping;
};

__END_SYS

#endif
