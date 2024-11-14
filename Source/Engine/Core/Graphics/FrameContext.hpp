#pragma once

#include "Core/Ptr.hpp"
#include <string>
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
