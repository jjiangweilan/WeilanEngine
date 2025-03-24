#include "AutoGrowLinearAllocator.hpp"

AutoGrowLinearAllocator::AutoGrowLinearAllocator(size_t initialSize)
{
    memSize = initialSize;
    mem = new unsigned char[initialSize];
}

AutoGrowLinearAllocator::~AutoGrowLinearAllocator()
{
    delete[] static_cast<unsigned char*>(mem);
}

AutoGrowLinearAllocator::AutoGrowLinearAllocator(AutoGrowLinearAllocator&& other)
    : mem(std::exchange(other.mem, nullptr)), memSize(std::exchange(other.memSize, 0)),
      offset(std::exchange(other.offset, 0))
{}

AutoGrowLinearAllocator& AutoGrowLinearAllocator::operator=(AutoGrowLinearAllocator&& other)
{
    mem = std::exchange(other.mem, nullptr);
    memSize = std::exchange(other.memSize, 0);
    offset = std::exchange(other.offset, 0);
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
        memcpy(tmp, mem, offset);
        delete[] static_cast<unsigned char*>(mem);
        mem = tmp;
    }
}
