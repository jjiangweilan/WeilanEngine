#include <atomic>
#include <spdlog/spdlog.h>

class Spinlock
{
public:
    Spinlock() {}

    void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire))
        {
        }
    }

    void unlock() { flag.clear(std::memory_order_release); }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

class ScopedSpinLock
{
public:
    ScopedSpinLock(Spinlock& lock) : lock(lock) { lock.lock(); }
    ~ScopedSpinLock() { lock.unlock(); }

private:
    Spinlock& lock;
};
