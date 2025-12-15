#pragma once
#include <vector>
#include "Assert.hpp"

template <class T>
class ObjectPool;

template <class T>
struct ObjectPoolHandle
{
    T* operator->()
    {
        ASSERT(index != -1 && "Dereferencing invalid handle");
        ASSERT(pool != nullptr && "Dereferencing handle with null pool");
        return &((*pool).allocatedObjects[index]);
    }

    const T* operator->() const
    {
        ASSERT(index != -1 && "Dereferencing invalid handle");
        ASSERT(pool != nullptr && "Dereferencing handle with null pool");
        return &((*pool).allocatedObjects[index]);
    }

    T& operator*()
    {
        ASSERT(index != -1 && "Dereferencing invalid handle");
        ASSERT(pool != nullptr && "Dereferencing handle with null pool");
        return (*pool).allocatedObjects[index];
    }

    const T& operator*() const
    {
        ASSERT(index != -1 && "Dereferencing invalid handle");
        ASSERT(pool != nullptr && "Dereferencing handle with null pool");
        return (*pool).allocatedObjects[index];
    }

    bool IsValid() const
    {
        return index != -1 && pool != nullptr;
    }

    ObjectPool<T>* pool;
    int index;
};

template <class T>
class ObjectPool
{
public:
    ObjectPool() : ObjectPool(16) {}

    ObjectPool(size_t poolSize)
    {
        allocatedObjects.resize(poolSize);
        isAllocated.resize(poolSize, false);
        freeIndices.reserve(poolSize);
        for (size_t i = 0; i < poolSize; ++i)
        {
            freeIndices.push_back(i);
        }
    }

    // Prevent copying (expensive and potentially dangerous with handles)
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Allow moving
    ObjectPool(ObjectPool&&) = default;
    ObjectPool& operator=(ObjectPool&&) = default;

    ObjectPoolHandle<T> Allocate()
    {
        if (freeIndices.empty())
        {
            Grow();
        }

        size_t index = freeIndices.back();
        freeIndices.pop_back();
        isAllocated[index] = true;
        return ObjectPoolHandle<T>{this, static_cast<int>(index)};
    }

    void Free(ObjectPoolHandle<T>& handle)
    {
        if (handle.pool != this)
        {
            ASSERT(false && "Attempting to free handle with wrong pool");
            return;
        }

        if (handle.index >= 0 && handle.index < static_cast<int>(allocatedObjects.size()))
        {
            if (!isAllocated[handle.index])
            {
                ASSERT(false && "Double-free detected");
                return;
            }

            isAllocated[handle.index] = false;
            freeIndices.push_back(static_cast<size_t>(handle.index));
            handle.index = -1;
        }
    }

    void Clear()
    {
        freeIndices.clear();
        for (size_t i = 0; i < allocatedObjects.size(); ++i)
        {
            freeIndices.push_back(i);
            isAllocated[i] = false;
        }
    }

    size_t GetCapacity() const { return allocatedObjects.size(); }
    size_t GetFreeCount() const { return freeIndices.size(); }
    size_t GetUsedCount() const { return allocatedObjects.size() - freeIndices.size(); }

private:
    void Grow()
    {
        size_t currentSize = allocatedObjects.size();
        size_t newSize = currentSize > 0 ? currentSize * 2 : 1;
        allocatedObjects.resize(newSize);
        isAllocated.resize(newSize, false);
        
        // Add new indices to freeIndices
        for (size_t i = currentSize; i < newSize; ++i)
        {
            freeIndices.push_back(i);
        }
    }

    std::vector<T> allocatedObjects;
    std::vector<size_t> freeIndices;
    std::vector<bool> isAllocated;

    template <class>
    friend class ObjectPoolHandle;
};
