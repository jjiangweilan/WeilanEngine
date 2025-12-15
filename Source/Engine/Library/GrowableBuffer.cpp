#include "GrowableBuffer.hpp"
#include "Engine/Library/Memory.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

GrowableBuffer::GrowableBuffer() : head(nullptr), totalSize(0), currentSize(0) {}

GrowableBuffer::~GrowableBuffer()
{
    if (head)
    {
        Platform_FreeMemory(head, false);
        head = nullptr;
    }
}

GrowableBuffer::GrowableBuffer(GrowableBuffer&& other) noexcept
    : head(other.head), totalSize(other.totalSize), currentSize(other.currentSize)
{
    other.head = nullptr;
    other.totalSize = 0;
    other.currentSize = 0;
}

GrowableBuffer& GrowableBuffer::operator=(GrowableBuffer&& other) noexcept
{
    if (this != &other)
    {
        if (head)
        {
            Platform_FreeMemory(head, false);
        }

        head = other.head;
        totalSize = other.totalSize;
        currentSize = other.currentSize;

        other.head = nullptr;
        other.totalSize = 0;
        other.currentSize = 0;
    }
    return *this;
}

size_t GrowableBuffer::Write(void* data, size_t size, size_t alignment)
{
    if (data == nullptr || size == 0)
        return 0;

    // Calculate aligned position
    size_t alignedPosition = AlignSize(currentSize, alignment);
    size_t requiredTotalSize = alignedPosition + size;

    // Ensure we have enough capacity
    EnsureCapacity(requiredTotalSize);

    uint8_t* rtn = static_cast<uint8_t*>(head) + alignedPosition;
    // Copy data to the aligned position
    std::memcpy(rtn, data, size);

    // Update current size
    currentSize = requiredTotalSize;

    return alignedPosition;
}

void GrowableBuffer::Reset()
{
    currentSize = 0;
}

void GrowableBuffer::Reserve(size_t capacity)
{
    if (capacity > totalSize)
    {
        Grow(capacity);
    }
}

size_t GrowableBuffer::GetSize() const
{
    return currentSize;
}

size_t GrowableBuffer::GetCapacity() const
{
    return totalSize;
}

void* GrowableBuffer::GetData() const
{
    return head;
}

void GrowableBuffer::EnsureCapacity(size_t requiredSize)
{
    if (requiredSize > totalSize)
    {
        // Calculate new size with growth strategy
        size_t newSize = std::max(totalSize * 2, requiredSize);

        // Ensure minimum capacity
        if (newSize < 64)
            newSize = 64;

        Grow(newSize);
    }
}

void GrowableBuffer::Grow(size_t newSize)
{
    if (newSize <= totalSize)
        return;

    void* newHead = Platform_ReAllocateMemory(head, newSize, false);
    if (newHead == nullptr)
    {
        throw std::bad_alloc();
    }

    head = newHead;
    totalSize = newSize;
}

size_t GrowableBuffer::AlignSize(size_t size, size_t alignment)
{
    if (alignment <= 1)
        return size;

    // Ensure alignment is a power of 2
    if ((alignment & (alignment - 1)) != 0)
    {
        throw std::invalid_argument("Alignment must be a power of 2");
    }

    return (size + alignment - 1) & ~(alignment - 1);
}
