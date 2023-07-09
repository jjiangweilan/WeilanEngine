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
    Gfx::ImageUsageFlags imageUsages;
    Gfx::BufferUsageFlags bufferUsages;
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
    virtual bool Execute(Gfx::CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack) { return true; }

    virtual void SetName(std::string_view name) { this->name = name; }
    std::string_view GetName() { return name; };
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
    Port* AddPort(const char* name,
                  Port::Type type,
                  const std::type_info& dataType,
                  bool isMultiPort = false,
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

    bool InsertBufferBarrierIfNeeded(ResourceStateTrack& stateTrack,
                                     ResourceRef* bufferRef,
                                     std::vector<Gfx::GPUBarrier>& barriers,
                                     Gfx::PipelineStageFlags stageFlags,
                                     Gfx::AccessMaskFlags accessFlags)
    {

        if (bufferRef)
        {
            auto& resourceState = stateTrack.GetState(bufferRef);
            if ((Gfx::HasWriteAccessMask(accessFlags) || Gfx::HasWriteAccessMask(resourceState.accessMask)) &&
                resourceState.accessMask != Gfx::AccessMask::None)
            {
                Gfx::Buffer* buf = (Gfx::Buffer*)bufferRef->GetVal();
                Gfx::GPUBarrier barrier;
                barrier.buffer = buf;
                barrier.bufferInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
                barrier.bufferInfo.srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
                barrier.bufferInfo.offset = 0;
                barrier.bufferInfo.size = buf->GetSize();
                barrier.srcStageMask = resourceState.stage;
                barrier.srcAccessMask = resourceState.accessMask;
                barrier.dstStageMask = stageFlags;
                barrier.dstAccessMask = accessFlags;

                barriers.push_back(barrier);

                resourceState.stage |= stageFlags;
                resourceState.accessMask |= accessFlags;
            }

            return true;
        }

        return false;
    }

    bool InsertImageBarrierIfNeeded(ResourceStateTrack& stateTrack,
                                    ResourceRef* imageRes,
                                    std::vector<Gfx::GPUBarrier>& barriers,
                                    Gfx::ImageLayout layout,
                                    Gfx::PipelineStageFlags stageFlags,
                                    Gfx::AccessMaskFlags accessFlags,
                                    std::optional<Gfx::ImageSubresourceRange> imageSubresourceRange = std::nullopt)
    {
        if (imageRes)
        {
            auto& resourceState = stateTrack.GetState(imageRes);
            if (resourceState.layout != layout)
            {
                Gfx::GPUBarrier barrier;
                Gfx::Image* image = (Gfx::Image*)imageRes->GetVal();
                barrier.image = image;
                barrier.imageInfo.srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
                barrier.imageInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
                barrier.imageInfo.oldLayout = resourceState.layout;
                barrier.imageInfo.newLayout = layout;
                barrier.imageInfo.subresourceRange = imageSubresourceRange.value_or(image->GetSubresourceRange());
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

    std::string name = "";

private:
    uint32_t depth = 0;
    std::list<std::unique_ptr<Port>> inputPorts;
    std::list<std::unique_ptr<Port>> outputPorts;
};
} // namespace Engine::RGraph
