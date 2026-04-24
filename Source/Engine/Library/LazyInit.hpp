#pragma once

#include <functional>
#include <memory>

template <class T>
struct LazyInit
{
    template <class... Args>
    LazyInit(Args&&... args)
    {
        construct = [=, this]() { return std::make_unique<T>(args...); };
    }

    T* operator->() { return Get(); }

    T* Get()
    {
        if (val == nullptr)
        {
            val = construct();
            construct = nullptr;
        }

        return val.get();
    }

    std::function<std::unique_ptr<T>()> construct;
    std::unique_ptr<T> val = nullptr;
};
