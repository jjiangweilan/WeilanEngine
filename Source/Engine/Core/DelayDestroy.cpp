#include "DelayDestroy.hpp"

DelayDestroy* DelayDestroy::Singleton()
{
    static DelayDestroy d;
    return &d;
}

void DelayDestroy::Destory(std::unique_ptr<Object>&& obj)
{
    pending.push_back(std::move(obj));
}

void DelayDestroy::Flush() {
    pending.clear();
}
