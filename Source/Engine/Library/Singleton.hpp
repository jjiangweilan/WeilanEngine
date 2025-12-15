#pragma once

#include <mutex>

template <typename T>
class Singleton
{
private:
    // Private static instance
    static std::unique_ptr<T> instance;
    static std::mutex mtx;

    // Private constructor to prevent instantiation
    Singleton() = default;

    // Delete the copy constructor and assignment operator to prevent cloning
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

public:
    // Static method to get the instance of the class
    static std::unique_ptr<T>& GetInstance()
    {
        // Double-checked locking mechanism
        if (instance == nullptr)
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (instance == nullptr)
            {
                instance = new T();
            }
        }
        return instance;
    }
};
