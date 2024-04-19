#pragma once
#include <memory>
#include <stdexcept>

template <class T>
class ManagedObject
{
public:
    ManagedObject() : object(nullptr), referenceCounter(new int(0)) {}
    explicit ManagedObject(T* object) : object(object), referenceCounter(new int(0)) {}
    ManagedObject(std::nullptr_t)
    {
        object = nullptr;
        referenceCounter = nullptr;
    }
    ManagedObject(const ManagedObject<T>& other) = delete;
    ManagedObject(std::unique_ptr<T>&& other)
    {
        object = other.release();
        referenceCounter = new int(0);
    }

    ManagedObject(ManagedObject<T>&& other)
    {
        *this = nullptr;

        object = std::exchange(other.object, nullptr);
        referenceCounter = std::exchange(other.referenceCounter, nullptr);
    }
    ~ManagedObject<T>()
    {
        if (referenceCounter)
        {
            Destroy();
        }
    }

    T* Get()
    {
        return object;
    }

    ManagedObject<T>& operator=(std::nullptr_t)
    {
        if (referenceCounter)
        {
            Destroy();
            object = nullptr;
            referenceCounter = nullptr;
        }

        return *this;
    }

    ManagedObject<T>& operator=(ManagedObject<T>&& other)
    {
        *this = nullptr;

        object = other.release();
        referenceCounter = new int(0);
    }

    bool operator==(std::nullptr_t) const
    {
        return object == nullptr;
    }

    T* operator->() const
    {
        return object;
    }

    T& operator*() const
    {
        return *object;
    }

    int GetReferenceCount() const
    {
        return *referenceCounter;
    }

    T* object = nullptr;
    int* referenceCounter;

    template <class U>
    friend class Ref;

private:
    inline void Destroy()
    {
        int originalCounterValue = *referenceCounter;
        if (originalCounterValue == 0)
            delete referenceCounter;
        else // must be greater than zero
        {
            // flip the counter value so that Ref knows this object is deleted
            *referenceCounter = -originalCounterValue;
        }

        if (object != nullptr)
            delete object;
    }
};

template <class T, class... Args>
ManagedObject<T> MakeManaged(Args&&... args)
{
    return ManagedObject<T>(std::forward(args)...);
}

template <class T>
class Ref
{
public:
    Ref() : object(nullptr), referenceCounter(nullptr) {}
    Ref(const ManagedObject<T>& managedObject)
        : object(managedObject.object), referenceCounter(managedObject.referenceCounter)
    {
        *referenceCounter += 1;
    }

    Ref(Ref<T>&& other)
    {
        object = std::exchange(other.object, nullptr);
        referenceCounter = std::exchange(other.referenceCounter, nullptr);
    }

    Ref(const Ref<T>& other) : object(other.object), referenceCounter(other.referenceCounter)
    {
        if (referenceCounter)
            *referenceCounter += 1;
    }

    ~Ref()
    {
        if (referenceCounter)
        {
            int originalCounterValue = Decrement();

            if (*referenceCounter == 0 && originalCounterValue < 0)
                delete referenceCounter;
        }
    }

    Ref<T>& operator=(std::nullptr_t)
    {
        if (referenceCounter)
        {
            int originalCounterValue = Decrement();

            if (*referenceCounter == 0 && originalCounterValue < 0)
                delete referenceCounter;
        }

        object = nullptr;
        referenceCounter = nullptr;

        return *this;
    }

    Ref<T>& operator=(const ManagedObject<T>& managedObject)
    {
        *this = nullptr;

        object = managedObject.object;
        referenceCounter = managedObject.referenceCounter;

        if (referenceCounter)
        {
            *referenceCounter += 1;
        }
    }

    Ref<T> operator=(Ref<T>&& other)
    {
        *this = nullptr;

        object = std::exchange(other.object, nullptr);
        referenceCounter = std::exchange(other.referenceCounter, nullptr);
    }

    Ref<T>& operator=(const Ref<T>& other)
    {
        if (object == other.object && referenceCounter == other.referenceCounter)
            return *this;

        *this = nullptr;

        object = other.object;
        referenceCounter = other.referenceCounter;

        if (referenceCounter)
            *referenceCounter += 1;

        return *this;
    }

    T* Get()
    {
        return object;
    }

    T& operator*()
    {
        if (*referenceCounter < 0)
            throw std::runtime_error("dereferencing a null object");

        return *object;
    }

    bool operator==(std::nullptr_t) const
    {
        return referenceCounter == nullptr || *referenceCounter < 0;
    }

    inline T* operator->() const
    {
        if (referenceCounter && *referenceCounter > 0)
            return object;

        throw std::runtime_error("dereferencing a null object");
    }

    inline T& operator*() const
    {
        if (referenceCounter && *referenceCounter > 0)
            return *object;

        throw std::runtime_error("dereferencing a null object");
    }

private:
    T* object = nullptr;
    int* referenceCounter = nullptr;

    inline int Decrement()
    {
        int originalCounterValue = *referenceCounter;
        *referenceCounter = originalCounterValue > 0 ? originalCounterValue - 1 : originalCounterValue + 1;

        return originalCounterValue;
    }

    friend class Serializer;
};
