#pragma once
#include "Core/Object.hpp"
#include "Core/ObjectTracker.hpp"
#include "Libs/UUID.hpp"
#include <cstddef>
#include <memory>

template <class T>
class ObjPtr
{
public:
    using element_type = T;

    ObjPtr() : handle(ObjectTracker::NullHandle) {}
    ObjPtr(Object* object)
    {
        if (object != nullptr)
            handle = ObjectTracker::Singleton().Track(object->GetUUID());
        else
            handle = ObjectTracker::NullHandle;
    };
    ObjPtr(const UUID& uuid) { handle = ObjectTracker::Singleton().Track(uuid); }
    ObjPtr(std::nullptr_t) { handle = ObjectTracker::NullHandle; }
    ObjPtr(const ObjPtr<T>& other) { handle = ObjectTracker::Singleton().Track(other.handle); }
    ~ObjPtr() { ObjectTracker::Singleton().Detrack(handle); }

    ObjPtr<T>& operator=(const ObjPtr<T>& other)
    {
        if (handle != ObjectTracker::NullHandle)
            ObjectTracker::Singleton().Detrack(handle);

        handle = ObjectTracker::Singleton().Track(other.handle);

        return *this;
    }

    ObjPtr<T>& operator=(const UUID& uuid)
    {
        if (handle != ObjectTracker::NullHandle)
            ObjectTracker::Singleton().Detrack(handle);

        handle = ObjectTracker::Singleton().Track(uuid);

        return *this;
    }

    ObjPtr<T>& operator=(T* object)
    {
        if (handle != ObjectTracker::NullHandle)
            ObjectTracker::Singleton().Detrack(handle);

        if (object == nullptr)
        {
            handle = ObjectTracker::NullHandle;
            return *this;
        }

        handle = ObjectTracker::Singleton().Track(object->GetUUID());

        return *this;
    }

    ObjPtr<T>& operator=(std::nullptr_t)
    {
        if (handle != ObjectTracker::NullHandle)
            ObjectTracker::Singleton().Detrack(handle);

        handle = ObjectTracker::NullHandle;

        return *this;
    }

    inline operator T*() const { return Get(); }
    inline bool operator==(std::nullptr_t) const { return Get() == nullptr; }
    inline bool operator!=(std::nullptr_t) const { return Get() != nullptr; }
    inline bool operator==(ObjPtr<T> other) const { return handle == other.handle; }
    inline bool operator!=(ObjPtr<T> other) const { return handle != other.handle; }

    inline T* operator->() const { return (T*)(ObjectTracker::Singleton().GetObject(handle)); }
    inline T& operator*() const { return *(T*)(ObjectTracker::Singleton().GetObject(handle)); }

    T* Get() const
    {
        auto obj = ObjectTracker::Singleton().GetObject(handle);

#if ENGINE_DEV_BUILD
        if (obj == nullptr)
        {
            return nullptr;
        }

        if constexpr (!std::is_same_v<T, Object>)
        {
            if (T::StaticGetObjectTypeID() != obj->GetObjectTypeID())
            {
                spdlog::critical("Invalid handle detected,  this shouldn't happen");
                return nullptr;
            }
        }
#endif

        return (T*)obj;
    }

private:
    ObjectTrackHandle handle;
};

template <class T>
struct IsObjPtr : public std::false_type
{};

template <class T>
struct IsObjPtr<ObjPtr<T>> : public std::true_type
{};

template <class T>
class RefPtr
{
public:
    using Type = T;
    RefPtr() = default;
    RefPtr(const std::unique_ptr<T>& ptr);
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
