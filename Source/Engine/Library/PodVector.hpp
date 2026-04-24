#pragma once
#include <utility>

template <class T>
class PodVector
{
public:
    PodVector() {}
    PodVector(size_t size) : m_Data(new T[size]), m_Size(size) {}
    PodVector(const PodVector& other) { *this = other; }
    PodVector(PodVector&& other) { *this = std::move(other); }
    PodVector& operator=(const PodVector& other)
    {
        resize(other.m_Size);

        for (auto curr = m_Data, src = other.m_Data; curr != m_Data + m_Size; ++curr, src++)
        {
            *curr = *src;
        }

        return *this;
    }
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

    template <class U>
    PodVector& operator=(U&& iterable)
    {
        size_t size = iterable.end() - iterable.begin();
        resize(size);

        auto curr = m_Data;
        for (auto iter = iterable.begin(); iter != iterable.end(); ++iter)
        {
            *curr = *iter;
            curr++;
        }

        return *this;
    }

    void resize(size_t size)
    {
        if (m_Size != size)
        {
            T* newData = new T[size];

            size_t copyTo = m_Size < size ? m_Size : size;
            for (auto curr = newData, src = m_Data; curr != newData + copyTo; curr++, src++)
            {
                new (curr) T(*src);
            }

            if (m_Data != nullptr)
            {
                delete[] m_Data;
            }

            m_Data = newData;
            m_Size = size;
        }
    }

    void ensure_size_uninitialized(size_t size)
    {
        T* newData = nullptr;

        if (m_Size < size)
        {
            newData = new T[size];
            delete[] m_Data;

            m_Data = newData;
            m_Size = size;
        }
    }

    T* data() { return m_Data; }
    size_t size() const { return m_Size; }

private:
    T* m_Data = nullptr;
    size_t m_Size = 0;
};
