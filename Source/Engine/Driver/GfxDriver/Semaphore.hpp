#pragma once
#include <string_view>
namespace Gfx
{
class Semaphore
{
public:
    struct CreateInfo
    {
        bool signaled;
    };

    virtual void SetName(std::string_view name) = 0;
    virtual ~Semaphore(){};
};
} // namespace Gfx
