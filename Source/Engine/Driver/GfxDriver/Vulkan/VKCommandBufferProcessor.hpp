#pragma once
#include "Engine/Driver/GfxDriver/Vulkan/RayTracing/VKRayTracing.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKContext.hpp"
#include "VKCommandBuffer.hpp"
#include "VKInflightCmd.hpp"
#include "VKResourceAllocator.hpp"
#include <variant>

namespace Gfx
{

struct VKResolvedTemporaryBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
    void* mappedData = nullptr;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct VKResolvedBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
    void* mappedData = nullptr;
    uint64_t offset = 0;
    uint64_t size = 0;
};
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

    std::vector<ResourceUsage> previousFrameUsages;
    std::vector<ResourceUsage> currentFrameUsages;
};

class VKCommandBufferProcessor
{
public:
    VKCommandBufferProcessor(int inflightCount, VKRayTracing::Manager* rayTracingManager);
    ~VKCommandBufferProcessor();

    void Execute(
        VKFramePrepareData& framePrepare,
        VKFrameContext& cmd,
        int inflightIndex,
        Queue& executionQueue,
        const GfxFeaturesSettings& featureSettings,
        CmdBufExecutionReport& report
    );

    VKImage* GetImage(const UUID& id);
    VKImage* Request(const ImageIdentifier& id, RenderImageDescriptor& desc);
    VKRenderPass* Request(RenderPass& renderPass);
    void ShaderReloaded();

    void FrameFinished(int inflightIndex);

    VKResolvedBuffer ResolveBuffer(BufferIdentifier identifier, int inflightIndex);

    struct VKBufferResourceRef
    {
        uint64_t trackingId = 0;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        VkDeviceSize size = VK_WHOLE_SIZE;
        VKBuffer* rawBuffer = nullptr;
    };

    VKBufferResourceRef ResolveTrackableBuffer(BufferIdentifier identifier, int inflightIndex);
    bool TrackResource(const VKBufferResourceRef& ref, VkPipelineStageFlags stages, VkAccessFlags access);
    int MakeBarrierForLastUsage(const VKBufferResourceRef& ref);

private:
    friend class VKResourceAllocator;
    struct ShaderBinding
    {
        ResourceType type;
        std::variant<ObjPtr<Image>, BufferIdentifier> res;
        std::optional<ImageViewOption> imageViewOption = std::nullopt;
        bool IsNull()
        {
            bool isNull = false;
            std::visit([&isNull](auto&& arg)
                       {
                if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, ObjPtr<Image>>)
                    isNull = arg.Get() == nullptr;
                else
                    isNull = !arg.IsBuffer();
                },
                       res);
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

        int dynamicBindSetCmdIndex[4];
        bool dynamicBindedSetUpdateNeeded[4] = {false, false, false, false};
    } recordState{};

    struct ExecutionState
    {
        struct SetResources
        {
            bool needUpdate = false;
            bool dynamicBindingNeedUpdate = false;
            VKShaderResource* resource = VK_NULL_HANDLE;
            int dynamicBindSetCmdIndex = -1;
        } setResources[4] = {};

        VkPipeline lastBindedPipeline = VK_NULL_HANDLE;
        VKShaderProgram* pendingBindedShader; // shader that is set to be binded
        VKShaderProgram* bindedShader;        // shader that is actually binded
        PipelineConfig shaderConfig;
        PipelineConfig pendingShaderConfig;
        VkDescriptorSet bindedDescriptorSets[4];
        VKBuffer* vertexBufferBindings[8];
        VkBuffer resolvedVertexBuffers[8];
        int vertexBufferBindingCount = 0;
        int subpassIndex = -1;
        VKRenderPass* renderPass;
        bool overrideViewport = false;
        bool overrideScissor = false;
        VkViewport currentViewport = {};
        VkRect2D currentScissor = {};
        int currentTimestapQueryIndex = 0;
    } exeState;

    struct DescriptorSetCacheInfo
    {
        VkDescriptorSet set;
        uint32_t setIndex;
        ObjPtr<VKShaderProgram> shaderProgram;
    };

    struct TransientDescriptorSetInfo
    {
        VKDescriptorPool* pool = nullptr;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    size_t previousActiveSchedulingCmdsSize;
    std::unordered_map<UUID, ResourceUsageTrack> resourceUsageTracks;
    std::unordered_map<uint64_t, ResourceUsageTrack> tempResourceUsageTracks;
    // odd frame activeSchedulingCmds and resource usages are cleared in next odd frame
    size_t evenRecordActiveSchedulingCmdsIndex;

    std::unique_ptr<VKBuffer> defaultUBOBuffer;
    std::unique_ptr<VKBuffer> defaultSSBOBuffer;
    std::vector<Barrier> barriers;
    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
    std::vector<VkMemoryBarrier> memoryBarriers;
    std::vector<std::shared_ptr<AsyncReadbackHandle>> asyncReadbacks;

    std::vector<VkWriteDescriptorSet> dynamicBindingCache_writes;
    std::vector<VkDescriptorBufferInfo> dynamicBindingCache_bufferInfos;
    std::vector<VkDescriptorImageInfo> dynamicBindingCache_imageInfos;
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> dynamicBinding_asWrites;
    std::vector<VkAccelerationStructureKHR> dynamicBinding_asHandles;

    using ShaderProgramID = UUID;
    std::unordered_map<ShaderProgramID, VKShaderResource> globalResources;
    std::unordered_map<ShaderBindingHandle, std::unordered_map<int, ShaderBinding>> globalResourcePool;

    std::unordered_map<uint64_t, DescriptorSetCacheInfo> descriptorSetCache;
    VKRayTracing::Manager* rayTracingManager;

    std::vector<std::unordered_map<uint64_t, VKResolvedTemporaryBuffer>> temporaryBuffers;
    std::vector<std::vector<TransientDescriptorSetInfo>> transientDescriptorSets;
    std::vector<uint32_t> hoistedTransferCmdIndices;

    VkDescriptorSet
    RequestDescriptorSet(std::span<VkWriteDescriptorSet> writes, uint32_t set, VKShaderProgram* shaderProgram);
    VkDescriptorSet
    RequestTransientDescriptorSet(std::span<VkWriteDescriptorSet> writes, uint32_t set, VKShaderProgram* shaderProgram, int inflightIndex);
    void CreateRenderPassNode(int visitIndex);
    // scheduling
    void FlushAllBindedSetUpdate(
        int inflightIndex, std::vector<VKCmd>& cmds, std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded
    );
    void MakeBarrierFromWritableResources(std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded, const std::vector<VKWritableGPUResource>& writableResources);
    void MakeBarrierForAllDynamicBindedSetUpdate(
        std::vector<VKCmd>& cmds, std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded
    );
    bool TrackResource(
        VKImage* writableResource,
        Gfx::ImageSubresourceRange range,
        VkImageLayout layout,
        VkPipelineStageFlags stages,
        VkAccessFlags access
    );
    bool TrackResource(VKBuffer* writableResource, VkPipelineStageFlags stages, VkAccessFlags access);
    int TrackUploadDataDestination(BufferIdentifier dst, int inflightIndex);
    void GoThroughRenderPass(
        int inflightIndex,
        std::vector<VKCmd>& exectedCmds,
        VKRenderPass& renderPass,
        int& visitIndex,
        int& barrierCount,
        int& barrierOffset,
        uint32_t& hoistedTransferOffset,
        uint32_t& hoistedTransferCount
    );
    void BindDynamicDescriptorSet(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VKDynamicBindResourceCmd& dynamicBindResourceCmd, uint32_t set, VKShaderProgram* shaderProgram, int inflightIndex);
    std::vector<VKWritableGPUResource> GetWritableResourcesNoCache(uint32_t set, VKDynamicBindResourceCmd& dynamicBindResourceCmd, VKShaderProgram* shaderProgram, VKCommandBufferProcessor* graph);
    size_t TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier);
    void FlushBindResourceTrack();
    int MakeBarrierForLastUsage(void* res, const UUID& resUUID);
    int MakeBarrierForLastUsage(VKImage* image);
    void RemoveImageRelatedInfo(Image* ptr);

    void ScheduleBindShaderProgram(VKCmd& cmd, int visitIndex);
    void TryBindShader(VkCommandBuffer cmd);
    void UpdateDescriptorSetBinding(int inflightIndex, VkCommandBuffer cmd, uint32_t index, VkPipelineBindPoint bindPoint);
    void UpdateDescriptorSetBinding(std::vector<VKCmd>& cmds, int inflightIndex, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint);
    void UpdateDynamicDescriptorSetBinding(std::vector<VKCmd>& cmds, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, int inflightIndex, int setIndex);
    void PutBarrier(VkCommandBuffer cmd, int index);
    void PutBarriers(VkCommandBuffer vkcmd, int barrierOffset, int barrierCount);
    void PreExecute(int inflightIndex, VKFramePrepareData& framePrepare);
    bool IsHoistedRenderPassTransferCmd(VKCmdType type) const;
    bool IsUnsupportedRenderPassTransferCmd(VKCmdType type) const;
    void AllocateTemporaryBufferCmd(VKAllocateBufferCmd& args, int inflightIndex);
    void PrepareCopyBufferCmd(VKCopyBufferCmd& args, int inflightIndex);
    void PrepareUploadDataCmd(VKUploadDataCmd& args, int inflightIndex);
    void ExecuteCopyBufferCmd(VkCommandBuffer vkcmd, VKCopyBufferCmd& args, int inflightIndex);
    void ExecuteUploadDataCmd(VkCommandBuffer vkcmd, VKUploadDataCmd& args, int inflightIndex);
    void ExecuteHoistedRenderPassTransfers(
        VkCommandBuffer vkcmd,
        std::vector<VKCmd>& executedCmds,
        uint32_t hoistedTransferOffset,
        uint32_t hoistedTransferCount,
        int inflightIndex
    );
    void BeginRenderPass(
        VkCommandBuffer vkcmd,
        VKRenderPass* renderPass,
        VkClearValue* clearValues,
        int clearValueCount,
        int barrierOffset,
        int barrierCount
    );
    void UpdateViewportAndScissorForRenderPass(VkCommandBuffer vkcmd, VkViewport viewport, VkRect2D scissor, Extent2D extent)
    {
        if (!exeState.overrideViewport &&
            (viewport.x != exeState.currentViewport.x ||
             viewport.y != exeState.currentViewport.y ||
             viewport.width != exeState.currentViewport.width ||
             viewport.height != exeState.currentViewport.height ||
             viewport.minDepth != exeState.currentViewport.minDepth ||
             viewport.maxDepth != exeState.currentViewport.maxDepth))
        {
            exeState.currentViewport = viewport;
            exeState.overrideViewport = false;
            vkCmdSetViewport(vkcmd, 0, 1, &viewport);
        }

        if (!exeState.overrideScissor &&
            (scissor.offset.x != exeState.currentScissor.offset.x ||
             scissor.offset.y != exeState.currentScissor.offset.y ||
             scissor.extent.width != exeState.currentScissor.extent.width ||
             scissor.extent.height != exeState.currentScissor.extent.height))
        {
            exeState.overrideScissor = false;
            exeState.currentScissor = scissor;
            vkCmdSetScissor(vkcmd, 0, 1, &scissor);
        }
    }

    void GetImageViewOrBufferOrAccelerationStructure(DynamicBinding& binding, VKImageView*& imageView, VKBuffer*& buffer, AccelerationStructureRef& asRef);

    int MakeBarrierForLastUsage2(VKImage* image)
    {
        auto iter = resourceUsageTracks.find(image->GetUUID());
        ASSERT(iter != resourceUsageTracks.end());

        int barrierCount = 0;
        auto& currentFrameUsages = iter->second.currentFrameUsages;
        auto& currentUsage = currentFrameUsages.back();

        if (image->GetName().find("SH2") != std::string::npos)
        {
            int i = 0;
        }

        auto barrier2s = image->MakeBarrierIfNeeded(currentUsage.stages, currentUsage.access, currentUsage.layout, MapVkImageSubresourceRange(currentUsage.range));

        for (auto& neededBarrier2 : barrier2s)
        {
            Barrier barrier;
            barrier.srcStageMask = neededBarrier2.srcStageMask;
            barrier.dstStageMask = neededBarrier2.dstStageMask;

            VkImageMemoryBarrier imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            imageBarrier.srcAccessMask = neededBarrier2.srcAccessMask;
            imageBarrier.dstAccessMask = neededBarrier2.dstAccessMask;
            imageBarrier.oldLayout = neededBarrier2.oldLayout;
            imageBarrier.newLayout = neededBarrier2.newLayout;
            imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.subresourceRange = neededBarrier2.subresourceRange;
            imageBarrier.image = image->GetImage();

            barrier.barrierCount = 1;
            barrier.imageMemorybarrierIndex = imageMemoryBarriers.size();
            barrier.targetImage = image;
            barriers.push_back(barrier);

            barrierCount += 1;
            imageMemoryBarriers.push_back(imageBarrier);
        }

        return barrierCount;
    }
};

VKImage* ImageIdentifier_GetImage(const Gfx::ImageIdentifier& id, VKCommandBufferProcessor* graph = nullptr);
VKImageView* ImageIdentifier_GetImageView(const Gfx::ImageIdentifier& id, VKCommandBufferProcessor* graph = nullptr);
} // namespace Gfx
