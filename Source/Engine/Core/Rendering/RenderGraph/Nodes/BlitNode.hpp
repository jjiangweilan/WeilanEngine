#pragma once
#include "../Node.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::RGraph
{
class BlitNode : public Node
{
public:
    BlitNode()
    {
        srcImageOut = AddPort("ImageSrc", Port::Type::Output, typeid(Gfx::Image));
        dstImageOut = AddPort("ImageDst", Port::Type::Output, typeid(Gfx::Image));

        srcImageIn = AddPort("ImageSrc",
                             Port::Type::Input,
                             typeid(Gfx::Image),
                             false,
                             [](Node* bNode, Port* other, auto type)
                             {
                                 BlitNode* node = (BlitNode*)bNode;

                                 if (type == Port::ConnectionType::Connect)
                                 {
                                     node->srcImageOut->SetResource(other->GetResource());
                                 }
                                 else node->srcImageOut->RemoveResource(other->GetResource());
                             });
        dstImageIn = AddPort("ImageDst",
                             Port::Type::Input,
                             typeid(Gfx::Image),
                             false,
                             [](Node* bNode, Port* other, auto type)
                             {
                                 BlitNode* node = (BlitNode*)bNode;

                                 if (type == Port::ConnectionType::Connect)
                                 {
                                     node->dstImageOut->SetResource(other->GetResource());
                                 }
                                 else node->dstImageOut->RemoveResource(other->GetResource());
                             });
    }

    bool Preprocess(ResourceStateTrack& stateTrack) override
    {
        stateTrack.GetState(srcImageIn->GetResource()).usages |= Gfx::ImageUsage::TransferSrc;
        stateTrack.GetState(dstImageIn->GetResource()).usages |= Gfx::ImageUsage::TransferDst;

        return true;
    }

    bool Execute(CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack) override
    {
        auto& srcImageState = stateTrack.GetState(srcImageIn->GetResource());
        auto& dstImageState = stateTrack.GetState(dstImageIn->GetResource());

        std::vector<GPUBarrier> barriers;
        InsertImageBarrierIfNeeded(stateTrack,
                                   srcImageIn->GetResource(),
                                   barriers,
                                   Gfx::ImageLayout::Transfer_Src,
                                   Gfx::PipelineStage::Transfer,
                                   Gfx::AccessMask::Memory_Read);
        InsertImageBarrierIfNeeded(stateTrack,
                                   dstImageIn->GetResource(),
                                   barriers,
                                   Gfx::ImageLayout::Transfer_Dst,
                                   Gfx::PipelineStage::Transfer,
                                   Gfx::AccessMask::Memory_Write);
        if (!barriers.empty())
        {
            cmdBuf->Barrier(barriers.data(), barriers.size());
        };

        cmdBuf->Blit((Gfx::Image*)srcImageIn->GetResourceVal(), (Gfx::Image*)dstImageIn->GetResourceVal());

        return true;
    }

    Port* GetPortSrcImageIn() { return srcImageIn; }
    Port* GetPortSrcImageOut() { return srcImageOut; }
    Port* GetPortDstImageIn() { return dstImageIn; }
    Port* GetPortDstImageOut() { return dstImageOut; }

private:
    Port* srcImageIn;
    Port* dstImageIn;

    Port* srcImageOut;
    Port* dstImageOut;
};
} // namespace Engine::RGraph
