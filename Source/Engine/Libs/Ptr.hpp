#pragma once

#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <memory>
#include <utility>

// managed pointer without multi-threading support
template <class T>
class UniPtr
{
public:
    using Type = T;
    UniPtr() = default;
    explicit UniPtr(T* ptr);
    UniPtr(std::nullptr_t);
    template <class U>
    UniPtr(UniPtr<U>&& other);
    UniPtr(UniPtr<T>&& other);
    UniPtr(const UniPtr<T>& other) = delete;
    template <class U>
    UniPtr<T>& operator=(UniPtr<U>&& other);
    UniPtr<T>& operator=(UniPtr<T>&& other);
    UniPtr<T>& operator=(std::nullptr_t);

    ~UniPtr();

    T* Get() const { return ptr; }
    inline void Release() { ptr = nullptr; }
    bool operator!=(std::nullptr_t) const { return ptr != nullptr; }
    bool operator==(std::nullptr_t) const { return ptr == nullptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }

private:
    T* ptr = nullptr;
    template <class U>
    friend class RefPtr;
    template <class U>
    friend class UniPtr;
};

template <class T>
class RefPtr
{
public:
    using Type = T;
    RefPtr() = default;
    RefPtr(const std::unique_ptr<T>& ptr);
    RefPtr(const UniPtr<T>& uniPtr);
    RefPtr(const RefPtr<T>& other);
    RefPtr(T* purePtr);
    ~RefPtr();

    template <class U>
    operator RefPtr<U>();
    template <class U>
    RefPtr(const RefPtr<U>& other);
    template <class U>
    RefPtr<T>& operator=(const RefPtr<U>& other);
    RefPtr<T>& operator=(std::nullptr_t);
    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    bool operator==(RefPtr<T> other) const;
    operator bool() const;
    T* Get() const { return ptr; }
    T*& GetPtrRef() { return ptr; }
    inline T* operator->() const { return ptr; }
    inline T& operator*() const { return *ptr; }

private:
    T* ptr = nullptr;

    template <class U>
    friend class RefPtr;
};

template <class T>
UniPtr<T>::UniPtr(T* ptr) : ptr(ptr)
{}

template <class T>
UniPtr<T>::UniPtr(std::nullptr_t) : ptr(nullptr)
{}

template <class T>
template <class U>
UniPtr<T>::UniPtr(UniPtr<U>&& other) : ptr(std::exchange(other.ptr, nullptr))
{}
template <class T>
UniPtr<T>::UniPtr(UniPtr<T>&& other) : ptr(std::exchange(other.ptr, nullptr))
{}

template <class T>
UniPtr<T>& UniPtr<T>::operator=(std::nullptr_t)
{
    if (ptr != nullptr)
    {
        delete ptr;
        ptr = nullptr;
    }

    return *this;
}

template <class T>
template <class U>
UniPtr<T>& UniPtr<T>::operator=(UniPtr<U>&& other)
{
    if (ptr != nullptr) delete ptr;
    ptr = std::exchange(other.ptr, nullptr);
    return *this;
}

template <class T>
UniPtr<T>& UniPtr<T>::operator=(UniPtr<T>&& other)
{
    if (ptr != nullptr) delete ptr;
    ptr = std::exchange(other.ptr, nullptr);
    return *this;
}

template <class T>
UniPtr<T>::~UniPtr()
{
    if (ptr != nullptr)
    {
        delete ptr;
        ptr = nullptr;
    }
}

template <class T, class... Args>
UniPtr<T> MakeUnique(Args&&... args)
{
    return UniPtr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
RefPtr<T>::RefPtr(const UniPtr<T>& uniPtr) : ptr(uniPtr.ptr)
{}

template <class T>
RefPtr<T>& RefPtr<T>::operator=(std::nullptr_t)
{
    ptr = nullptr;

    return *this;
}

template <class T>
RefPtr<T>::RefPtr(T* purePtr) : ptr(purePtr)
{}

template <class T>
RefPtr<T>::operator bool() const
{
    return ptr != nullptr;
}

template <class T>
template <class U>
RefPtr<T>& RefPtr<T>::operator=(const RefPtr<U>& other)
{
    ptr = static_cast<T*>(other.ptr);

    return *this;
}

template <class T>
template <class U>
RefPtr<T>::RefPtr(const RefPtr<U>& other) : ptr(static_cast<T*>(other.ptr))
{}

template <class T>
RefPtr<T>::RefPtr(const RefPtr<T>& other)
{
    ptr = other.ptr;
}

template <class T>
bool RefPtr<T>::operator==(std::nullptr_t) const
{
    return ptr == nullptr;
}

template <class T>
bool RefPtr<T>::operator!=(std::nullptr_t) const
{
    return ptr != nullptr;
}

template <class T>
RefPtr<T>::RefPtr(const std::unique_ptr<T>& ptr) : ptr(ptr.get())
{}

template <class T>
template <class U>
RefPtr<T>::operator RefPtr<U>()
{
    return RefPtr<U>(static_cast<U*>(ptr));
}

template <class T>
bool RefPtr<T>::operator==(RefPtr<T> other) const
{
    return ptr == other.ptr;
}

template <class T>
RefPtr<T>::~RefPtr()
{}
