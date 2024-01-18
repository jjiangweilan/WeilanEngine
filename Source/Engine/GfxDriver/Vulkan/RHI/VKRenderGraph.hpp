#pragma once
#include "../VKCommandBuffer2.hpp"

namespace Gfx::VK::RenderGraph
{
enum class RenderingNodeType
{
    RenderPass,
    Compute
};

struct RenderPassNode
{
    int cmdBegin;
    int cmdEnd;
    int barrierOffset;
    int barrierCount;
};

struct RenderingNode
{
    RenderingNodeType type;

    union
    {
        RenderPassNode renderPass;
    };
};

enum class ResourceType
{
    Image,
    Buffer
};

struct ResourceUsage
{
    VkPipelineStageFlags stages;
    VkAccessFlags access;
    int usageIndex;

    // valid when it's a image resource
    Gfx::ImageSubresourceRange range;
    VkImageLayout layout;
};

struct ResourceUsageTrack
{
    ResourceType type;

    void* res;

    std::vector<ResourceUsage> usages;
};

class Graph
{
public:
    void Schedule(VKCommandBuffer2& cmd);

    void Execute();

private:
    struct FlushBindResource
    {
        int bindProgramIndex;
        int bindSetCmdIndex[4];
        bool bindedSetUpdateNeeded[4] = {false, false, false, false};
    } state;

    std::vector<VKCmd> activeSchedulingCmds;
    std::vector<RenderingNode> renderingNodes;
    std::unordered_map<void*, ResourceUsageTrack> resourceUsageTracks;
    std::vector<Barrier> barriers;

    void CreateRenderPassNode(int visitIndex);
    void TrackResource(
        int renderingOpIndex,
        ResourceType type,
        void* writableResource,
        Gfx::ImageSubresourceRange range,
        VkImageLayout layout,
        VkPipelineStageFlags stages,
        VkAccessFlags access
    );
    void RegisterRenderPass(int& visitIndex);
    void FlushBindResourceTrack();
    int MakeBarrier(void* res, int usageIndex);
};
} // namespace Gfx::VK::RenderGraph
