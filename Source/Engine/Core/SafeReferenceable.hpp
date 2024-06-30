#pragma once
#include <memory>
#include <stdexcept>
template <class T>
class SRef
{
public:
    SRef(std::nullptr_t) : object(nullptr), referenceCount(nullptr) {}

    SRef() : object(nullptr), referenceCount(nullptr) {}

    SRef(T* object, std::shared_ptr<int> referenceCount) : object(object), referenceCount(referenceCount)
    {
        *referenceCount += 1;
    }

    SRef(SRef<T>&& other)
    {
        object = std::exchange(other.object, nullptr);
        referenceCount = std::exchange(other.referenceCount, nullptr);
    }

    SRef(const SRef<T>& other)
    {
        if (referenceCount)
        {
            int originalValue = *referenceCount;
            if (originalValue > 0)
                *referenceCount = originalValue - 1;
            else
                *referenceCount = originalValue + 1;
        }

        object = other.object;
        referenceCount = other.referenceCount;

        if (referenceCount)
        {
            if (*referenceCount > 0)
                *referenceCount += 1;
            else
                *referenceCount -= 1;
        }
    }

    SRef& operator=(SRef<T>&& other) noexcept
    {
        if (referenceCount)
        {
            int originalValue = *referenceCount;
            if (originalValue > 0)
                *referenceCount = originalValue - 1;
            else
                *referenceCount = originalValue + 1;
        }

        object = std::exchange(other.object, nullptr);
        referenceCount = std::exchange(other.referenceCount, nullptr);

        return *this;
    }

    SRef& operator=(const SRef<T>& other)
    {
        object = other.object;
        referenceCount = other.referenceCount;

        if (referenceCount)
        {
            if (*referenceCount > 0)
                *referenceCount += 1;
            else
                *referenceCount -= 1;
        }

        return *this;
    }

    ~SRef()
    {
        if (referenceCount)
        {
            int originalReferenceCount = *referenceCount;
            if (originalReferenceCount < 0)
                *referenceCount += 1;
            else
                *referenceCount -= 1;
        }
    }

    bool operator==(std::nullptr_t)
    {
        if (referenceCount)
            return *referenceCount < 0;

        return true;
    }

    operator bool()
    {
        if (referenceCount)
            return *referenceCount > 0;

        return false;
    }

    T* Get() const
    {
        if (referenceCount && *referenceCount > 0)
            return object;

        return nullptr;
    }

    T* operator->()
    {
        if (referenceCount && *referenceCount > 0)
            return object;

        return object;
    }

    T& operator*()
    {
        if (referenceCount == nullptr || *referenceCount < 0)
            throw std::runtime_error("dereferencing a destroied object");

        return *object;
    }

private:
    T* object = nullptr;
    // when the referenced object is alive referenceCount >= 0
    // when the renfereced object is destroyed referenceCount < 0
    std::shared_ptr<int> referenceCount = nullptr;
};

template <class T>
class SafeReferenceable
{
public:
    virtual ~SafeReferenceable()
    {
        if (referenceCount)
        {
            *referenceCount = -*referenceCount;
        }
    }

    SRef<T> GetSRef()
    {
        if (referenceCount == nullptr)
            referenceCount = std::make_shared<int>(0);

        return SRef<T>(static_cast<T*>(this), referenceCount);
    }

    template <class U>
        requires std::derived_from<U, T>
    SRef<U> GetSRef()
    {
        if (referenceCount == nullptr)
            referenceCount = std::make_shared<int>(0);

        return SRef<U>(static_cast<U*>(this), referenceCount);
    }

private:
    std::shared_ptr<int> referenceCount = nullptr;
};

namespace std
{
template <class T>
struct hash<SRef<T>>
{
    size_t operator()(const SRef<T>& x) const
    {
        return std::hash(x.Get());
    }
};
} // namespace std
