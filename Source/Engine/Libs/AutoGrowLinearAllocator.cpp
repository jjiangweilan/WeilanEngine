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

void AutoGrowLinearAllocator::GrowIfNeeded(size_t targetSize)
{
    if (targetSize > memSize)
    {
        size_t oldMemSize = memSize;
        while (targetSize < memSize)
        {
            memSize *= 2;
        }

        unsigned char* tmp = new unsigned char[memSize];
        memcpy(tmp, mem, oldMemSize);
        delete[] static_cast<unsigned char*>(mem);
        mem = tmp;
    }
}
