#pragma once
#include "../Node.hpp"
namespace Engine::RGraph
{
class MemoryTransferNode : public Node
{
public:
    MemoryTransferNode() : in(InitIn()), out(InitOut()) {}

    bool Preprocess(ResourceStateTrack& stateTrack) override
    {
        out.srcBuffer->SetResource(in.srcBuffer->GetResource());
        out.dstBuffer->SetResource(in.dstBuffer->GetResource());
        out.srcImage->SetResource(in.srcImage->GetResource());
        out.dstImage->SetResource(in.dstImage->GetResource());

        if (in.srcImage->GetResource())
        {
            auto& s = stateTrack.GetState(in.srcImage->GetResource());
            s.imageUsages |= Gfx::ImageUsage::TransferSrc;
        }

        if (in.dstImage->GetResource())
        {
            auto& s = stateTrack.GetState(in.dstImage->GetResource());
            s.imageUsages |= Gfx::ImageUsage::TransferDst;
        }

        if (in.srcBuffer->GetResource())
        {
            auto& s = stateTrack.GetState(in.srcBuffer->GetResource());
            s.bufferUsages |= Gfx::BufferUsage::Transfer_Src;
        }

        if (in.dstBuffer->GetResource())
        {
            auto& s = stateTrack.GetState(in.dstBuffer->GetResource());
            s.bufferUsages |= Gfx::BufferUsage::Transfer_Dst;
        }
        return true;
    }

    bool Execute(Gfx::CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack) override
    {
        barriers.clear();

        Gfx::Image* srcImage = (Gfx::Image*)in.srcImage->GetResourceVal();
        Gfx::Image* dstImage = (Gfx::Image*)in.dstImage->GetResourceVal();
        Gfx::Buffer* srcBuffer = (Gfx::Buffer*)in.srcBuffer->GetResourceVal();
        Gfx::Buffer* dstBuffer = (Gfx::Buffer*)in.dstBuffer->GetResourceVal();

        // image to buffer
        if (srcImage && dstBuffer)
        {
            InsertBufferBarrierIfNeeded(stateTrack,
                                        in.dstBuffer->GetResource(),
                                        barriers,
                                        Gfx::PipelineStage::Transfer,
                                        Gfx::AccessMask::Transfer_Write);
            InsertImageBarrierIfNeeded(stateTrack,
                                       in.srcImage->GetResource(),
                                       barriers,
                                       Gfx::ImageLayout::Transfer_Src,
                                       Gfx::PipelineStage::Transfer,
                                       Gfx::AccessMask::Transfer_Read);

            if (!barriers.empty())
            {
                cmdBuf->Barrier(barriers.data(), barriers.size());
            }

            Gfx::ImageSubresourceRange range = srcImage->GetSubresourceRange();
            Gfx::ImageSubresourceLayers layers{.aspectMask = range.aspectMask,
                                               .mipLevel = range.baseMipLevel,
                                               .baseArrayLayer = range.baseArrayLayer,
                                               .layerCount = range.layerCount};

            Gfx::BufferImageCopyRegion region{
                .srcOffset = 0,
                .layers = layers,
                .offset = {0, 0},
                .extend = {srcImage->GetDescription().width, srcImage->GetDescription().height, 1},
            };

            Gfx::BufferImageCopyRegion regions[] = {region};
            cmdBuf->CopyImageToBuffer(srcImage, dstBuffer, regions);
        }
        else if (srcBuffer && dstImage)
        {
            InsertBufferBarrierIfNeeded(stateTrack,
                                        in.srcBuffer->GetResource(),
                                        barriers,
                                        Gfx::PipelineStage::Transfer,
                                        Gfx::AccessMask::Transfer_Read);
            InsertImageBarrierIfNeeded(stateTrack,
                                       in.dstImage->GetResource(),
                                       barriers,
                                       Gfx::ImageLayout::Transfer_Dst,
                                       Gfx::PipelineStage::Transfer,
                                       Gfx::AccessMask::Transfer_Write);

            if (!barriers.empty())
            {
                cmdBuf->Barrier(barriers.data(), barriers.size());
            }

            Gfx::ImageSubresourceRange range = dstImage->GetSubresourceRange();
            Gfx::ImageSubresourceLayers layers{.aspectMask = range.aspectMask,
                                               .mipLevel = range.baseMipLevel,
                                               .baseArrayLayer = range.baseArrayLayer,
                                               .layerCount = range.layerCount};
            Gfx::BufferImageCopyRegion region{
                .srcOffset = 0,
                .layers = layers,
                .offset = {0, 0},
                .extend = {dstImage->GetDescription().width, dstImage->GetDescription().height, 1},
            };

            Gfx::BufferImageCopyRegion regions[] = {region};
            cmdBuf->CopyBufferToImage(srcBuffer, dstImage, regions);
        }

        return true;
    }

    const struct In
    {
        Port* srcBuffer;
        Port* dstBuffer;
        Port* srcImage;
        Port* dstImage;
    } in;

    const struct Out
    {
        Port* srcBuffer;
        Port* dstBuffer;
        Port* srcImage;
        Port* dstImage;
    } out;

private:
    std::vector<Gfx::GPUBarrier> barriers;

    In InitIn()
    {
        In in;
        in.srcBuffer = AddPort("Src Buffer", Port::Type::Input, typeid(Gfx::Buffer));
        in.dstBuffer = AddPort("Dst Buffer", Port::Type::Input, typeid(Gfx::Buffer));
        in.srcImage = AddPort("Src Image", Port::Type::Input, typeid(Gfx::Image));
        in.dstImage = AddPort("Dst Image", Port::Type::Input, typeid(Gfx::Image));

        return in;
    }

    Out InitOut()
    {
        Out out;

        out.srcBuffer = AddPort("Src Buffer", Port::Type::Output, typeid(Gfx::Buffer));
        out.dstBuffer = AddPort("Dst Buffer", Port::Type::Output, typeid(Gfx::Buffer));
        out.srcImage = AddPort("Src Image", Port::Type::Output, typeid(Gfx::Image));
        out.dstImage = AddPort("Dst Image", Port::Type::Output, typeid(Gfx::Image));

        return out;
    }
};
} // namespace Engine::RGraph
