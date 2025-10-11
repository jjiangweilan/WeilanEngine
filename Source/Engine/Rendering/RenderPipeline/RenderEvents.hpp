#pragma once

namespace Rendering
{
enum class RenderEvents
{
    None = -1,
    Deferred,
    ForwardOpaque,
    ForwardTransparent,
    MAX_COUNT
};
}
