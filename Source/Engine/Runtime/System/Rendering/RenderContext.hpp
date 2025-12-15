#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"

namespace Rendering
{
// pass render information across passes
struct RenderingContext
{
    bool isPreZEnabled;
};
} // namespace Rendering
