#include "../Memory.hpp"

class ThreadLocalAllocator : public std::pmr::memory_resource
{
public:
    ThreadLocalAllocator() = default;
    ~ThreadLocalAllocator() override { GetStackAllocator().Reset(); }

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        return GetStackAllocator().Allocate(bytes, alignment);
    }

    // void do_deallocate(T* p, std::size_t n) { GetStackAllocator().Deallocate((unsigned char*)p, n * sizeof(T)); }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
    {
        return GetStackAllocator().Deallocate(p, bytes, alignment);
    }

    // all LocalStackMemoryAllocator instances are wrapper of the global GetStackAllocator()
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return true; }
};
