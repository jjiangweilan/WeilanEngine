#include "Memory.hpp"
#include "Engine/Library/LazyInit.hpp"

class MemoryManager
{
public:
    static MemoryManager& Instance()
    {
        static MemoryManager instance;
        return instance;
    }
    auto& GetTLSStackAllocator() { return *stackAllocator.Get(); }

private:
    static thread_local LazyInit<StackAllocator> stackAllocator;
};

thread_local LazyInit<StackAllocator> MemoryManager::stackAllocator(1024 * 1024 * 1); // 1 MB

StackAllocator& GetStackAllocator()
{
    return MemoryManager::Instance().GetTLSStackAllocator();
}

StackAllocator& GetSharedStackAllocator()
{
    static StackAllocator stackAllocator(1024 * 1024 * 1);
    return stackAllocator;
}
