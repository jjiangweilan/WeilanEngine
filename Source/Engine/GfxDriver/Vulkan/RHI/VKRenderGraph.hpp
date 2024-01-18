#pragma once
#include "../VKCommandBuffer2.hpp"

namespace Gfx::VK::RenderGraph
{
enum class RenderingOperationType
{
    RenderPass,
    Compute
};

struct RenderingOperation
{
    RenderingOperationType type;
};

enum class ResourceType
{
    Image,
    Buffer
};

struct ResourceUsageTrack
{
    using UsageIndex = int; // index to renderingOperations
    ResourceType type;

    void* res; // ImageView when type is ResourceType::Image

    std::vector<UsageIndex> usages;
};

class Graph
{
public:
    void Schedule(VKCommandBuffer2& cmd);

    void Execute();

private:
    struct FlushBindResource
    {
        size_t bindedProgram;
        size_t bindedResources[4];
    } state;

    std::span<VKCmd> activeSchedulingCmds;
    std::vector<RenderingOperation> renderingOperations;
    std::unordered_map<void*, ResourceUsageTrack> resourceUsageTracks;
    void CreateRenderPassNode(int visitIndex);
    void RegisterRenderPass(int& visitIndex);
    void FlushBindResourceTrack();
};
} // namespace Gfx::VK::RenderGraph
