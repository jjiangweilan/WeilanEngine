#pragma once
#include "../VKCommandBuffer.hpp"
#include "../VKInflightCmd.hpp"
#include "GfxDriver/Vulkan/VKContext.hpp"
#include <variant>

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

    std::variant<ObjPtr<Image>, ObjPtr<Buffer>> res;

    DynamicArray<ResourceUsage> previousFrameUsages;
    DynamicArray<ResourceUsage> currentFrameUsages;
};

class Graph
{
public:
    Graph(int inflightCount);
    ~Graph();

    void Execute(
        VKFramePrepareData& framePrepare,
        VKInflightCmd& cmd,
        int inflightIndex,
        Queue& executionQueue,
        const GfxFeaturesSettings& featureSettings,
        CmdBufExecutionReport& report
    );

    VKImage* GetImage(const UUID& id);
    VKImage* Request(const RG::ImageIdentifier& id, RG::ImageDescription& desc);
    VKRenderPass* Request(RG::RenderPass& renderPass);

private:
    class ResourceAllocator;
    struct ShaderBinding
    {
        ResourceType type;
        std::variant<ObjPtr<Image>, ObjPtr<Buffer>> res;
        std::optional<ImageViewOption> imageViewOption = std::nullopt;
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
        PipelineConfig config{};
        int bindProgramIndex{};
        int bindSetCmdIndex[4];
        bool bindedSetUpdateNeeded[4] = {false, false, false, false};
    } recordState{};

    struct ExecutionState
    {
        struct SetResources
        {
            bool needUpdate = false;
            VKShaderResource* resource = VK_NULL_HANDLE;
        } setResources[4] = {};

        VKShaderProgram* lastBindedShader; // shader that is set to be binded
        VKShaderProgram* bindedShader;     // shader that is actually binded
        PipelineConfig shaderConfig;
        PipelineConfig lastShaderConfig;
        VkDescriptorSet bindedDescriptorSets[4];
        VKBuffer* vertexBufferBindings[8];
        int vertexBufferBindingCount = 0;
        int subpassIndex = -1;
        VKRenderPass* renderPass;
        bool overrideViewport = false;
        bool overrideScissor = false;
        int currentTimestapQueryIndex = 0;
    } exeState;

    size_t previousActiveSchedulingCmdsSize;
    std::unordered_map<UUID, ResourceUsageTrack> resourceUsageTracks;
    // odd frame activeSchedulingCmds and resource usages are cleared in next odd frame
    size_t evenRecordActiveSchedulingCmdsIndex;

    DynamicArray<Barrier> barriers;
    DynamicArray<VkImageMemoryBarrier> imageMemoryBarriers;
    DynamicArray<VkBufferMemoryBarrier> bufferMemoryBarriers;
    DynamicArray<VkMemoryBarrier> memoryBarriers;
    DynamicArray<std::shared_ptr<AsyncReadbackHandle>> asyncReadbacks;

    using ShaderProgramID = UUID;
    std::unordered_map<ShaderProgramID, VKShaderResource> globalResources;
    std::unordered_map<ShaderBindingHandle, std::unordered_map<int, ShaderBinding>> globalResourcePool;
    // std::unique_ptr<VKShaderResource> globalResource;
    //
    std::unique_ptr<ResourceAllocator> resourceAllocator;

    void CreateRenderPassNode(int visitIndex);
    // scheduling
    void FlushAllBindedSetUpdate(
        DynamicArray<VKCmd>& cmds, DynamicArray<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded
    );
    bool TrackResource(
        VKImage* writableResource,
        Gfx::ImageSubresourceRange range,
        VkImageLayout layout,
        VkPipelineStageFlags stages,
        VkAccessFlags access
    );
    bool TrackResource(VKBuffer* writableResource, VkPipelineStageFlags stages, VkAccessFlags access);
    void GoThroughRenderPass(
        DynamicArray<VKCmd>& exectedCmds,
        VKRenderPass& renderPass,
        int& visitIndex,
        int& barrierCount,
        int& barrierOffset
    );
    size_t TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier);
    void FlushBindResourceTrack();
    int MakeBarrierForLastUsage(void* res, const UUID& resUUID);

    void ScheduleBindShaderProgram(VKCmd& cmd, int visitIndex);
    void TryBindShader(VkCommandBuffer cmd);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd, uint32_t index, VkPipelineBindPoint bindPoint);
    void UpdateDescriptorSetBinding(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint);
    void PutBarrier(VkCommandBuffer cmd, int index);
    void PreExecute(VKFramePrepareData& framePrepare);
};

Gfx::VKImage* ImageIdentifier_GetImage(
    const Gfx::RG::ImageIdentifier& id, Gfx::VK::RenderGraph::Graph* graph = nullptr
);
Gfx::VKImageView* ImageIdentifier_GetImageView(
    const Gfx::RG::ImageIdentifier& id, Gfx::VK::RenderGraph::Graph* graph = nullptr
);
} // namespace Gfx::VK::RenderGraph
