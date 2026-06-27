#include "DelayDestroy.hpp"

DelayDestroy* DelayDestroy::Singleton()
{
    static DelayDestroy d;
    return &d;
}

void DelayDestroy::Flush()
{
    auto deleting = std::move(pending);
    pending.clear();
}
