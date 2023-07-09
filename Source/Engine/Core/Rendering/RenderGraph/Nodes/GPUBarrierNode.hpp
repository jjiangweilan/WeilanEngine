#pragma once
#include "../Node.hpp"

namespace Engine::RGraph
{
class GPUBarrierNode : public Node
{
public:
    GPUBarrierNode() { imagePort = AddPort("Image", Port::Type::Input, typeid(Gfx::Image)); }

    bool Execute(Gfx::CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack) override
    {

        if (imagePort->GetResource())
        {
            std::vector<Gfx::GPUBarrier> barriers;
            InsertImageBarrierIfNeeded(stateTrack,
                                       imagePort->GetResource(),
                                       barriers,
                                       layout,
                                       stageFlags,
                                       accessFlags,
                                       imageSubresourceRange);

            if (!barriers.empty())
            {
                cmdBuf->Barrier(barriers.data(), barriers.size());
            }
        }

        return true;
    }

    Port* GetPortImageIn() { return imagePort; }

    Gfx::ImageLayout layout;
    Gfx::PipelineStageFlags stageFlags;
    Gfx::AccessMaskFlags accessFlags;
    std::optional<Gfx::ImageSubresourceRange> imageSubresourceRange = std::nullopt;

private:
    Port* imagePort;
};
} // namespace Engine::RGraph
