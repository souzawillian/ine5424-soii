// EPOS Thread Abstraction Implementation

#include <system/kmalloc.h>
#include <machine.h>
#include <thread.h>

// This_Thread class attributes
__BEGIN_UTIL
bool This_Thread::_not_booting;
__END_UTIL

__BEGIN_SYS

// Class attributes
Scheduler_Timer * Thread::_timer;

Thread* volatile Thread::_running;
Thread::Queue Thread::_ready;
Thread::Queue Thread::_suspended;

// Methods
void Thread::constructor_prolog(unsigned int stack_size)
{
    lock();

    _stack = reinterpret_cast<char *>(kmalloc(stack_size));
}


void Thread::constructor_epilog(const Log_Addr & entry, unsigned int stack_size)
{
    db<Thread>(TRC) << "Thread(entry=" << entry
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",s=" << stack_size
                    << "},context={b=" << _context
                    << "," << *_context << "}) => " << this << endl;

    switch(_state) {
        case RUNNING: break;
        case SUSPENDED: _suspended.insert(&_link); break;
        default: _ready.insert(&_link);
    }

    unlock();
}


Thread::~Thread()
{
    lock();

    db<Thread>(TRC) << "~Thread(this=" << this
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",context={b=" << _context
                    << "," << *_context << "})" << endl;

    _ready.remove(this);
    _suspended.remove(this);

    //joined
    if(_running == this && this->_join_wait) {
        Thread * t = this->_join_wait;
        _suspended.remove(&t->_link);
        t->_state = READY;
        _ready.insert(&t->_link);
        this->_join_wait = 0;
    }

    //joiner
    if(_running->_join_wait == this){
        _running->_join_wait  = 0;
    }

    /**
     * Remove this thread from waiting queue
     * when it was destructed.
     */
    if (_waiting)
        _waiting->remove(this);

    unlock();

    kfree(_stack);
}


int Thread::join()
{
    lock();

    db<Thread>(TRC) << "Thread::join(this=" << this << ",state=" << _state << ")" << endl;

    assert(_running != this);
    assert(!_join_wait);

    if(_state != FINISHING) {
        //yield(); // implicit unlock() Old code
        _join_wait = this;
        _join_wait->suspend();
    }

    unlock();

    return *reinterpret_cast<int *>(_stack);
}


void Thread::pass()
{
    lock();

    db<Thread>(TRC) << "Thread::pass(this=" << this << ")" << endl;

    Thread * prev = _running;
    prev->_state = READY;
    _ready.insert(&prev->_link);

    _ready.remove(this);
    _state = RUNNING;
    _running = this;

    dispatch(prev, this);

    unlock();
}

void Thread::sleep(Queue * sleeping)
{
    lock();

    Thread* thread = running();
    thread->_state = WAITING; // update state of current thread to waiting
    thread->_waiting = sleeping; // get a copy from pointer to thread sleeping queue 
    sleeping->insert(&thread->_link); // insert running thread, that going to sleep

    if(!_ready.empty()) {
        /**
         * If has not thread in ready queue, make one to running
         * updating its state and changing context
         */
        _running = _ready.remove()->object();
        _running->_state = RUNNING;

        dispatch(thread, _running);
    } else
        idle(); // implicit unlock()

    unlock();
}

void Thread::wakeup(Queue * sleeping)
{
    lock();
    if(!sleeping->empty()) {
        /**
         * If has sleep queue is non empty, wake up it
         * changing state to READY and adding to _ready queue
         * being able of be scheduled
         *
         * Restart waiting queue from this thread, changing to null (0)
         */
        Thread * t = sleeping->remove()->object();
        t->_state = READY;
        t->_waiting = 0;
        _ready.insert(&t->_link);
    }
    unlock();
};

void Thread::wakeup_all(Queue * sleeping) {
    lock();
    while(!sleeping->empty()) {
        /**
         * Wakeup each thread at sleeping queue
         */
        Thread * t = sleeping->remove()->object();
        t->_state = READY;
        t->_waiting = 0;
        _ready.insert(&t->_link);
    }
    unlock();
};

void Thread::suspend()
{
    lock();

    db<Thread>(TRC) << "Thread::suspend(this=" << this << ")" << endl;

    if(_running != this)
        _ready.remove(this);

    _state = SUSPENDED;
    _suspended.insert(&_link);

    if((_running == this) && !_ready.empty()) {
        _running = _ready.remove()->object();
        _running->_state = RUNNING;

        dispatch(this, _running);
    } else
        idle(); // implicit unlock()

    unlock();
}


void Thread::resume()
{
    lock();

    db<Thread>(TRC) << "Thread::resume(this=" << this << ")" << endl;

   _suspended.remove(this);
   _state = READY;
   _ready.insert(&_link);

   unlock();
}


// Class methods
void Thread::yield()
{
    lock();

    db<Thread>(TRC) << "Thread::yield(running=" << _running << ")" << endl;

    if(!_ready.empty()) {
        Thread * prev = _running;
        prev->_state = READY;
        _ready.insert(&prev->_link);

        _running = _ready.remove()->object();
        _running->_state = RUNNING;

        dispatch(prev, _running);
    } else
        idle();

    unlock();
}


void Thread::exit(int status)
{
    lock();

    db<Thread>(TRC) << "Thread::exit(status=" << status << ") [running=" << running() << "]" << endl;

    while(_ready.empty() && !_suspended.empty())
        idle(); // implicit unlock();

    lock();

    if(_running->_join_wait) {
        Thread * t = _running->_join_wait;
        _suspended.remove(&t->_link);
        t->_state = READY;
        _ready.insert(&t->_link);
        _running->_join_wait = 0;
    }

    if(!_ready.empty()) {
        Thread * prev = _running;
        prev->_state = FINISHING;
        *reinterpret_cast<int *>(prev->_stack) = status;

        _running = _ready.remove()->object();
        _running->_state = RUNNING;

        dispatch(prev, _running);
    } else {
        db<Thread>(WRN) << "The last thread in the system has exited!" << endl;
        if(reboot) {
            db<Thread>(WRN) << "Rebooting the machine ..." << endl;
            Machine::reboot();
        } else {
            db<Thread>(WRN) << "Halting the CPU ..." << endl;
            CPU::halt();
        }
    }

    unlock();
}


void Thread::reschedule()
{
    yield();
}


void Thread::time_slicer(const IC::Interrupt_Id & i)
{
    reschedule();
}


void Thread::dispatch(Thread * prev, Thread * next)
{
    if(prev != next) {
        if(prev->_state == RUNNING)
            prev->_state = READY;
        next->_state = RUNNING;

        db<Thread>(TRC) << "Thread::dispatch(prev=" << prev << ",next=" << next << ")" << endl;
        db<Thread>(INF) << "prev={" << prev << ",ctx=" << *prev->_context << "}" << endl;
        db<Thread>(INF) << "next={" << next << ",ctx=" << *next->_context << "}" << endl;

        CPU::switch_context(&prev->_context, next->_context);
    }

    unlock();
}


int Thread::idle()
{
    db<Thread>(TRC) << "Thread::idle()" << endl;

    db<Thread>(INF) << "There are no runnable threads at the moment!" << endl;
    db<Thread>(INF) << "Halting the CPU ..." << endl;

    CPU::int_enable();
    CPU::halt();

    return 0;
}

__END_SYS

// Id forwarder to the spin lock
__BEGIN_UTIL
unsigned int This_Thread::id()
{
    return _not_booting ? reinterpret_cast<volatile unsigned int>(Thread::self()) : Machine::cpu_id() + 1;
}
__END_UTIL
