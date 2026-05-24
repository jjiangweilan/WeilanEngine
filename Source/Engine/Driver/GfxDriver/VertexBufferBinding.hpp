#pragma once
#include "BufferIdentifier.hpp"
#include <inttypes.h>
namespace Gfx
{
struct VertexBufferBinding
{
    BufferIdentifier buffer;
    uint64_t offset;
};
} // namespace Gfx
