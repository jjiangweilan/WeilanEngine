#pragma once
#include "../VKCommandBuffer2.hpp"

namespace
{
class VKDriver;
}
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
    bool operator==(const ResourceUsage& other) const = default;
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

    std::vector<ResourceUsage> previousFrameUsages;
    std::vector<ResourceUsage> currentFrameUsages;
};

class Graph
{
public:
    Graph();
    void Schedule(VKCommandBuffer2& cmd);

    void Execute(VkCommandBuffer cmd);

private:
    struct ShaderBinding
    {
        ResourceType type;
        void* res;
    };

    struct FlushBindResource
    {
        VKShaderProgram* bindedProgram = nullptr;
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

    std::vector<VKCmd> previousSchedulingCmds;
    std::vector<VKCmd> currentSchedulingCmds;
    size_t previousActiveSchedulingCmdsSize;
    std::unordered_map<void*, ResourceUsageTrack> resourceUsageTracks;
    // odd frame activeSchedulingCmds and resource usages are cleared in next odd frame
    size_t evenRecordActiveSchedulingCmdsIndex;

    std::vector<Barrier> barriers;
    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
    std::vector<VkMemoryBarrier> memoryBarriers;

    std::unordered_map<ShaderProgram*, VKShaderResource> globalResources;
    std::unordered_map<ResourceHandle, ShaderBinding> globalResourcePool;
    // std::unique_ptr<VKShaderResource> globalResource;

    void CreateRenderPassNode(int visitIndex);
    bool TrackResource(
        VKImage* writableResource,
        Gfx::ImageSubresourceRange range,
        VkImageLayout layout,
        VkPipelineStageFlags stages,
        VkAccessFlags access
    );
    bool TrackResource(VKBuffer* writableResource, VkPipelineStageFlags stages, VkAccessFlags access);
    void AddBarrierToRenderPass(int& visitIndex);
    size_t TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier);
    void FlushBindResourceTrack();
    int MakeBarrierForLastUsage(void* res);

    void ScheduleBindShaderProgram(VKCmd& cmd, int visitIndex);
    void TryBindShader(VkCommandBuffer cmd);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd, uint32_t index);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd);
    void PutBarrier(VkCommandBuffer cmd, int index);
};
} // namespace Gfx::VK::RenderGraph
