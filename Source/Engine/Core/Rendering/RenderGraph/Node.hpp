#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Port.hpp"
#include <list>
#include <memory>

namespace Engine::RGraph
{
struct ResourceState
{
    Gfx::PipelineStageFlags stage;
    Gfx::AccessMaskFlags accessMask;
    Gfx::ImageLayout layout;
    int queueFamilyIndex;

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
    ResourceState& GetState(ResourceRef ref) { return resourceStateMap[ref]; };

private:
    std::unordered_map<ResourceRef, ResourceState, ResourceRefHash> resourceStateMap;
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
    virtual bool Execute(ResourceStateTrack& stateTrack) { return true; }

    void AddDepth(uint32_t delta)
    {
        depth += delta;
        for (auto& o : outputPorts)
        {
            Port* connected = o->GetConnectedPort();
            if (connected)
            {
                connected->GetNode()->AddDepth(delta);
            }
        }
    }

protected:
    Port* AddPort(const char* name, Port::Type type, const std::type_info& dataType)
    {
        auto port = new Port(this, name, type, dataType);

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
                if (Port* connected = port->GetConnectedPort())
                {
                    port->Disconnect();
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

    template <class T>
    T* GetResourceFromPort(Port* port)
    {
        if (port)
        {
            auto res = port->GetResource();
            if (!res.IsNull())
            {
                return res.GetVal();
            }
        }

        return nullptr;
    }

    void* GetResourceFromConnected(Port* port)
    {
        if (port)
        {
            auto connected = port->GetConnectedPort();
            if (connected)
            {
                auto res = connected->GetResource();
                if (!res.IsNull())
                {
                    return res.GetVal();
                }
            }
        }

        return nullptr;
    }

private:
    uint32_t depth = 0;
    std::list<std::unique_ptr<Port>> inputPorts;
    std::list<std::unique_ptr<Port>> outputPorts;
};
} // namespace Engine::RGraph
