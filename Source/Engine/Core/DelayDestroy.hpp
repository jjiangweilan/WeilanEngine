#pragma once
#include <memory>
#include "Object.hpp"

// This really should be replaced by a solid object management system
class DelayDestroy
{
public:
    static DelayDestroy* Singleton();
    void Destory(std::unique_ptr<Object>&& obj);
    void Flush();

private:
    std::vector<std::unique_ptr<Object>> pending;
};
