#pragma once
#include "../VKCommandBuffer.hpp"
#include <variant>

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

    std::variant<SRef<Image>, SRef<Buffer>> res;

    std::vector<ResourceUsage> previousFrameUsages;
    std::vector<ResourceUsage> currentFrameUsages;
};

class Graph
{
public:
    Graph();
    ~Graph();
    void Schedule(VKCommandBuffer& cmd);

    void Execute(VkCommandBuffer cmd);

    VKImage* GetImage(const UUID& id);
    VKImage* Request(RG::ImageIdentifier& id, RG::ImageDescription& desc);
    VKRenderPass* Request(RG::RenderPass& renderPass);

private:
    class ResourceAllocator;
    struct ShaderBinding
    {
        ResourceType type;
        std::variant<SRef<Image>, SRef<Buffer>> res;
        bool IsNull()
        {
            bool isNull = false;
            std::visit([&isNull](auto&& arg) { isNull = arg.Get() == nullptr; }, res);
            return isNull;
        }
    };

    struct RecordState
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
        bool overrideViewport = false;
        bool overrideScissor = false;
    } exeState;

    std::vector<VKCmd> currentSchedulingCmds;
    size_t previousActiveSchedulingCmdsSize;
    std::unordered_map<UUID, ResourceUsageTrack> resourceUsageTracks;
    // odd frame activeSchedulingCmds and resource usages are cleared in next odd frame
    size_t evenRecordActiveSchedulingCmdsIndex;

    std::vector<Barrier> barriers;
    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
    std::vector<VkMemoryBarrier> memoryBarriers;
    std::vector<std::shared_ptr<AsyncReadbackHandle>> asyncReadbacks;

    std::unordered_map<ShaderProgram*, VKShaderResource> globalResources;
    std::unordered_map<ShaderBindingHandle, std::unordered_map<int, ShaderBinding>> globalResourcePool;
    // std::unique_ptr<VKShaderResource> globalResource;
    //
    std::unique_ptr<ResourceAllocator> resourceAllocator;

    void CreateRenderPassNode(int visitIndex);
    // scheduling
    void FlushAllBindedSetUpdate(std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded);
    bool TrackResource(
        VKImage* writableResource,
        Gfx::ImageSubresourceRange range,
        VkImageLayout layout,
        VkPipelineStageFlags stages,
        VkAccessFlags access
    );
    bool TrackResource(VKBuffer* writableResource, VkPipelineStageFlags stages, VkAccessFlags access);
    void GoThroughRenderPass(VKRenderPass& renderPass, int& visitIndex, int& barrierCount, int& barrierOffset);
    size_t TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier);
    void FlushBindResourceTrack();
    int MakeBarrierForLastUsage(void* res, const UUID& resUUID);

    void ScheduleBindShaderProgram(VKCmd& cmd, int visitIndex);
    void TryBindShader(VkCommandBuffer cmd);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd, uint32_t index, VkPipelineBindPoint bindPoint);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint);
    void PutBarrier(VkCommandBuffer cmd, int index);
};
} // namespace Gfx::VK::RenderGraph
