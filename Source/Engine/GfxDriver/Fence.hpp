#pragma once

namespace Engine::Gfx
{
class Fence
{
public:
    struct CreateInfo
    {
        bool signaled = false;
    };
    virtual void Reset() = 0;
    virtual ~Fence(){};
};
} // namespace Engine::Gfx
