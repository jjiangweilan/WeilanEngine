#pragma once
#include <memory>
#include <vector>

// This really should be replaced by a solid object management system
class DelayDestroy
{
public:
    static DelayDestroy* Singleton();

    template <typename T, typename Deleter>
    void Destory(std::unique_ptr<T, Deleter>&& obj)
    {
        if (obj != nullptr)
        {
            pending.push_back(std::make_unique<PendingDestroy<std::unique_ptr<T, Deleter>>>(std::move(obj)));
        }
    }

    void Flush();

private:
    struct PendingDestroyBase
    {
        virtual ~PendingDestroyBase() = default;
    };

    template <typename Ptr>
    struct PendingDestroy : PendingDestroyBase
    {
        explicit PendingDestroy(Ptr&& ptr) : ptr(std::move(ptr)) {}

        Ptr ptr;
    };

    std::vector<std::unique_ptr<PendingDestroyBase>> pending;
};
