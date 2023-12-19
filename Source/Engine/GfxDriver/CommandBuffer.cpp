#include "CommandBuffer.hpp"

namespace Gfx
{
void CommandBuffer::BindSubmesh(const Submesh& submesh)
{
    std::vector<VertexBufferBinding> bindings;
    for (auto& b : submesh.GetBindings())
    {
        bindings.push_back({submesh.GetVertexBuffer(), b.byteOffset});
    }
    BindVertexBuffer(bindings, 0);
    BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
}
} // namespace Gfx
