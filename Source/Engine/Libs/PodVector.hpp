#pragma once
#include <utility>

template <class T>
class PodVector
{
public:
    PodVector() {}
    PodVector(size_t size) : m_Data(new T[size]), m_Size(size) {}
    PodVector(const PodVector& other) = delete;
    PodVector(PodVector&& other) { *this = std::move(other); }
    PodVector& operator=(const PodVector& other) = delete;
    PodVector& operator=(PodVector&& other)
    {
        if (m_Data != nullptr)
        {
            delete[] m_Data;
        }

        m_Data = other.m_Data;
        m_Size = other.m_Size;

        other.m_Data = nullptr;
        other.m_Size = 0;

        return *this;
    }
    ~PodVector()
    {
        if (m_Data)
        {
            delete[] m_Data;
        }
    }

    T* data() { return m_Data; }
    size_t size() const { return m_Size; }

private:
    T* m_Data = nullptr;
    size_t m_Size = 0;
};
