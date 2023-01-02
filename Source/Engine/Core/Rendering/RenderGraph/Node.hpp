#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Port.hpp"
#include <list>
#include <memory>

namespace Engine::RGraph
{
struct ResourceState
{
    Gfx::PipelineStageFlags stage = Gfx::PipelineStage::Bottom_Of_Pipe;
    Gfx::AccessMaskFlags accessMask = Gfx::AccessMask::None;
    Gfx::ImageLayout layout = Gfx::ImageLayout::Undefined;
    uint32_t queueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;

    /**
     * any one uses this resource should append it's usage
     */
    Gfx::ImageUsageFlags usages;
};
/**
 * Passed through Execute, all the resources stack are tracked by this class
 */
class ResourceStateTrack
{
public:
    ResourceState& GetState(ResourceRef* ref) { return resourceStateMap[ref->GetUUID()]; };

private:
    std::unordered_map<UUID, ResourceState> resourceStateMap;
};
class Node
{
public:
    virtual ~Node() {}
    uint32_t GetDepth() { return depth; }
    const std::list<std::unique_ptr<Port>>& GetInputPorts() { return inputPorts; }
    const std::list<std::unique_ptr<Port>>& GetOutputPorts() { return outputPorts; }

    virtual bool Preprocess(ResourceStateTrack& stateTrack) { return true; }
    virtual bool Compile(ResourceStateTrack& stateTrack) { return true; }
    virtual bool Execute(CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack) { return true; }

    void AddDepth(uint32_t delta)
    {
        depth += delta;
        for (auto& o : outputPorts)
        {
            std::span<Port*> connected = o->GetConnectedPorts();
            for (auto c : connected)
            {
                c->GetNode()->AddDepth(delta);
            }
        }
    }

protected:
    Port* AddPort(const char* name, Port::Type type, const std::type_info& dataType, bool isMultiPort = false,
                  Port::ConnectionCallBack connectionCallback = nullptr)
    {
        auto port = new Port(this, name, type, dataType, isMultiPort, connectionCallback);

        switch (port->GetType())
        {
            case Port::Type::Output: outputPorts.emplace_back(port); break;
            case Port::Type::Input: inputPorts.emplace_back(port); break;
        }

        return port;
    }

    void RemovePort(Port* port)
    {
        auto f = [port](const std::unique_ptr<Port>& other)
        {
            if (port == other.get())
            {
                for (auto c : port->GetConnectedPorts())
                {
                    c->Disconnect(port);
                }
                return true;
            }
            return false;
        };
        switch (port->GetType())
        {
            case Port::Type::Output: outputPorts.remove_if(f);
            case Port::Type::Input: inputPorts.remove_if(f);
        }
    }

    bool InsertImageBarrierIfNeeded(ResourceStateTrack& stateTrack, ResourceRef* imageRes,
                                    std::vector<GPUBarrier>& barriers, Gfx::ImageLayout layout,
                                    Gfx::PipelineStageFlags stageFlags, Gfx::AccessMaskFlags accessFlags,
                                    std::optional<Gfx::ImageSubresourceRange> imageSubresourceRange = std::nullopt)
    {
        if (imageRes)
        {
            auto& resourceState = stateTrack.GetState(imageRes);
            if (resourceState.layout != layout)
            {
                GPUBarrier barrier;
                barrier.image = (Gfx::Image*)imageRes->GetVal();
                barrier.imageInfo.srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
                barrier.imageInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
                barrier.imageInfo.oldLayout = resourceState.layout;
                barrier.imageInfo.newLayout = layout;
                barrier.imageInfo.subresourceRange =
                    imageSubresourceRange.value_or(((Gfx::Image*)imageRes->GetVal())->GetSubresourceRange());
                barrier.dstStageMask = stageFlags;
                barrier.dstAccessMask = accessFlags;
                barrier.srcStageMask = resourceState.stage;
                barrier.srcAccessMask = resourceState.accessMask;

                barriers.push_back(barrier);

                // set the new state
                resourceState.stage |= stageFlags;
                resourceState.layout = layout;
                resourceState.accessMask |= accessFlags;
                resourceState.queueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
                return true;
            }
        }

        return false;
    }

private:
    uint32_t depth = 0;
    std::list<std::unique_ptr<Port>> inputPorts;
    std::list<std::unique_ptr<Port>> outputPorts;
};
} // namespace Engine::RGraph
