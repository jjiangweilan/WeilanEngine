#include "AutoGrowLinearAllocator.hpp"

AutoGrowLinearAllocator::AutoGrowLinearAllocator(size_t initialSize)
{
    memSize = initialSize;
    remainingSize = memSize;
    mem = new unsigned char[initialSize];
    offset = mem;
}

AutoGrowLinearAllocator::~AutoGrowLinearAllocator()
{
    delete[] static_cast<unsigned char*>(mem);
}

AutoGrowLinearAllocator::AutoGrowLinearAllocator(AutoGrowLinearAllocator&& other)
    : mem(std::exchange(other.mem, nullptr)), offset(std::exchange(other.offset, nullptr)),
      memSize(std::exchange(other.memSize, 0)), remainingSize(std::exchange(other.remainingSize, 0))
{
    other.mem = new unsigned char[1024];
    other.memSize = 1024;
    other.remainingSize = other.memSize;
    other.offset = other.mem;
}

AutoGrowLinearAllocator& AutoGrowLinearAllocator::operator=(AutoGrowLinearAllocator&& other)
{
    mem = std::exchange(other.mem, nullptr);
    memSize = std::exchange(other.memSize, 0);
    offset = std::exchange(other.offset, nullptr);
    other.mem = new unsigned char[1024];
    other.memSize = 1024;
    other.remainingSize = other.memSize;
    return *this;
}

void AutoGrowLinearAllocator::GrowIfNeeded(size_t targetSize)
{
    if (targetSize > memSize)
    {
        while (targetSize > memSize)
        {
            memSize *= 2;
        }

        unsigned char* tmp = new unsigned char[memSize];
        memcpy(tmp, mem, (unsigned char*)offset - (unsigned char*)mem);
        delete[] static_cast<unsigned char*>(mem);
        mem = tmp;
    }
}
