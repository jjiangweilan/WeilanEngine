#include "Memory.hpp"

class MemoryManager
{
public:
    static MemoryManager& Instance()
    {
        static MemoryManager instance;
        return instance;
    }
    auto& GetTLSStackAllocator() { return stackAllocator; }

private:
    static thread_local StackAllocator stackAllocator; // 1 MB stack allocator
};

thread_local StackAllocator MemoryManager::stackAllocator(1024 * 1024 * 8); // 8 MB

StackAllocator& GetStackAllocator()
{
    return MemoryManager::Instance().GetTLSStackAllocator();
}

