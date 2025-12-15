#pragma once

#include <cstdint>
template <class T>
class UnorderedList
{
public:
    UnorderedList(uint32_t size = 0)
    {
        if (size != 0)
        {
            elements = new T[size];
        }
        else
        {
            elements = nullptr;
        }
    }

private:
    int size;
    int capacity;
    T* elements;
};
