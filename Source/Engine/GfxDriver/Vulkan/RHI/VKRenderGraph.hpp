#pragma once
#include "../VKCommandBuffer2.hpp"

namespace Gfx::VK::RenderGraph
{
struct RenderPassNode
{
    int cmdBegin;
    int cmdEnd;
    int barrierOffset;
    int barrierCount;
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

    void Execute(VkCommandBuffer cmd);

private:
    struct FlushBindResource
    {
        int bindProgramIndex;
        int bindSetCmdIndex[4];
        bool bindedSetUpdateNeeded[4] = {false, false, false, false};
    } recordState;

    struct ExecutionState
    {
        struct SetResources
        {
            bool needUpdate = false;
            VKShaderResource* resource = VK_NULL_HANDLE;
        } setResources[4] = {};

        VKShaderProgram* lastBindedShader; // shader that is set to be binded
        VKShaderProgram* bindedShader;     // shader that is actually binded
        const ShaderConfig* shaderConfig;
        VkDescriptorSet bindedDescriptorSets[4];
        int subpassIndex = -1;
        VKRenderPass* renderPass;
    } exeState;

    std::vector<VKCmd> activeSchedulingCmds;
    std::unordered_map<void*, ResourceUsageTrack> resourceUsageTracks;

    std::vector<Barrier> barriers;
    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
    std::vector<VkMemoryBarrier> memoryBarriers;

    void CreateRenderPassNode(int visitIndex);
    void TrackResource(
        ResourceType type,
        void* writableResource,
        Gfx::ImageSubresourceRange range,
        VkImageLayout layout,
        VkPipelineStageFlags stages,
        VkAccessFlags access
    );
    void AddBarrierToRenderPass(int& visitIndex);
    void FlushBindResourceTrack();
    int MakeBarrier(void* res, int usageIndex);

    void TryBindShader(VkCommandBuffer cmd);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd, uint32_t index);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd);
    void PutBarrier(VkCommandBuffer cmd, int index);
};
} // namespace Gfx::VK::RenderGraph
