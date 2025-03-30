#include "DelayDestroy.hpp"

DelayDestroy* DelayDestroy::Singleton()
{
    static DelayDestroy d;
    return &d;
}

void DelayDestroy::Destory(std::unique_ptr<Object>&& obj)
{
    if (obj != nullptr)
        pending.push_back(std::move(obj));
}

void DelayDestroy::Flush()
{
    pending.clear();
}
