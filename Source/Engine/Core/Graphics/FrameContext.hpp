#pragma once

#include "Libs/Ptr.hpp"
#include <string>
namespace Engine
{
namespace Gfx
{
class ShaderResource;
}
class FrameContext
{
public:
    virtual ~FrameContext() {}
    virtual RefPtr<Gfx::ShaderResource> GetShaderResource();

private:
};
} // namespace Engine