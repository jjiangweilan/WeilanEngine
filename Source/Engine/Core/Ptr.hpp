#pragma once

#include "Core/Object.hpp"
#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <memory>
#include <utility>

class ObjectLifetimeManager
{
public:
    static ObjectLifetimeManager* Singleton();
    void ScheduleDeletion(Object* object) { pending.push_back(object); }
    void Flush()
    {
        for (auto o : pending)
        {
            delete o;
        }
    };

private:
    std::vector<Object*> pending;
};

// managed pointer without multi-threading support
template <class T>
class ObjPtr
{
public:
    operator std::unique_ptr<T>()
    {
        std::unique_ptr<T> p(ptr);
        ptr = nullptr;
        return p;
    }
    using Type = T;
    ObjPtr() = default;
    explicit ObjPtr(T* ptr);
    ObjPtr(std::nullptr_t);
    template <class U>
    ObjPtr(ObjPtr<U>&& other);
    ObjPtr(ObjPtr<T>&& other);
    ObjPtr(const ObjPtr<T>& other) = delete;
    template <class U>
    ObjPtr<T>& operator=(ObjPtr<U>&& other);
    ObjPtr<T>& operator=(ObjPtr<T>&& other);
    ObjPtr<T>& operator=(std::nullptr_t);

    ~ObjPtr();

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
    RefPtr(const ObjPtr<T>& uniPtr);
    RefPtr(const RefPtr<T>& other);
    RefPtr(T* purePtr);
    ~RefPtr();

    template <class U>
    RefPtr(const RefPtr<U>& other);
    template <class U>
    RefPtr<T>& operator=(const RefPtr<U>& other);
    RefPtr<T>& operator=(std::nullptr_t);
    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    bool operator==(RefPtr<T> other) const;
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
ObjPtr<T>::ObjPtr(T* ptr) : ptr(ptr)
{}

template <class T>
ObjPtr<T>::ObjPtr(std::nullptr_t) : ptr(nullptr)
{}

template <class T>
template <class U>
ObjPtr<T>::ObjPtr(ObjPtr<U>&& other) : ptr(std::exchange(other.ptr, nullptr))
{}
template <class T>
ObjPtr<T>::ObjPtr(ObjPtr<T>&& other) : ptr(std::exchange(other.ptr, nullptr))
{}

template <class T>
ObjPtr<T>& ObjPtr<T>::operator=(std::nullptr_t)
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
ObjPtr<T>& ObjPtr<T>::operator=(ObjPtr<U>&& other)
{
    if (ptr != nullptr)
        delete ptr;
    ptr = std::exchange(other.ptr, nullptr);
    return *this;
}

template <class T>
ObjPtr<T>& ObjPtr<T>::operator=(ObjPtr<T>&& other)
{
    if (ptr != nullptr)
        delete ptr;
    ptr = std::exchange(other.ptr, nullptr);
    return *this;
}

template <class T>
ObjPtr<T>::~ObjPtr()
{
    if (ptr != nullptr)
    {
        ObjectLifetimeManager::Singleton()->ScheduleDeletion(ptr);
        ptr = nullptr;
    }
}

template <class T, class... Args>
ObjPtr<T> MakeObj(Args&&... args)
{
    return ObjPtr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
RefPtr<T>::RefPtr(const ObjPtr<T>& uniPtr) : ptr(uniPtr.ptr)
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
bool RefPtr<T>::operator==(RefPtr<T> other) const
{
    return ptr == other.ptr;
}

template <class T>
RefPtr<T>::~RefPtr()
{}
