#pragma once
#include <inttypes.h>
namespace Gfx
{
class Buffer;
struct VertexBufferBinding
{
    Gfx::Buffer* buffer;
    uint64_t offset;
};
} // namespace Gfx
