#pragma once

#include "../Node.hpp"

namespace Engine::RGraph
{
class BufferNode : public Node
{
public:
    BufferNode() : out(InitOut()), bufferRes(ResourceRef(typeid(Gfx::Buffer))) { out.buffer->SetResource(&bufferRes); }
    bool Compile(ResourceStateTrack& stateTrack) override;

    struct Props
    {
        size_t size = -1;
        bool visibleInCPU = false;
        const char* debugName = nullptr;
    } props;

    const struct Out
    {
        Port* buffer;
    } out;

private:
    UniPtr<Gfx::Buffer> buffer = nullptr;
    ResourceRef bufferRes;

    Out InitOut()
    {
        Out out;
        out.buffer = AddPort("Buffer", Port::Type::Output, typeid(Gfx::Buffer));

        return out;
    }
};
} // namespace Engine::RGraph
