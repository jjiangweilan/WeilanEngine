#include "VKRenderTarget.hpp"
#include "Internal/VKAppWindow.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKUtils.hpp"
#include "VKContext.hpp"
#include <cassert>
namespace Engine::Gfx
{
struct RenderTargetCreateHelper
{
public:
    VkFramebufferCreateInfo frameBufferCreateInfo;

    RenderTargetCreateHelper(RenderTargetDescription* rtDescription, VKAppWindow* window)
        : colorAttachmentCount(rtDescription->GetColorAttachmentCount()), window(window),
          hasDepthStencil(rtDescription->HasDepth()), description(rtDescription)
    {}

    VkRenderPassCreateInfo MakeRenderPassCreateInfo(const RenderPassConfig& renderPassConfig)
    {
        VkRenderPassCreateInfo renderPassCreateInfo;
        MakeRenderPassCreateInfos(*description, renderPassConfig);
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.pNext = VK_NULL_HANDLE;
        renderPassCreateInfo.flags = 0;
        renderPassCreateInfo.attachmentCount = attachmentCount;
        renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
        renderPassCreateInfo.subpassCount = subpassDescriptions.size();
        renderPassCreateInfo.pSubpasses = subpassDescriptions.data();
        renderPassCreateInfo.dependencyCount = 0;
        renderPassCreateInfo.pDependencies = VK_NULL_HANDLE;
        return renderPassCreateInfo;
    }

    VkFramebufferCreateInfo MakeFrameBufferCreateInfo(VkRenderPass renderPass,
                                                      VkImageView* imageViews,
                                                      uint32_t imageViewCount)
    {
        VkFramebufferCreateInfo createInfo;

        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = 0;
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = attachmentCount;
        createInfo.pAttachments = imageViews;
        createInfo.width = description->width == 0 ? window->GetDefaultWindowSize().width : description->width;
        createInfo.height = description->height == 0 ? window->GetDefaultWindowSize().height : description->height;
        createInfo.layers = 1;

        return createInfo;
    }

private:
    RenderTargetDescription* description;
    VKAppWindow* window;
    size_t colorAttachmentCount;
    bool hasDepthStencil;

    size_t attachmentCount;

    // render pass data
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<VkSubpassDescription> subpassDescriptions;
    std::vector<VkAttachmentReference> colorAttachmentReferences;
    VkAttachmentReference depthStencilAttachmentReference;

    void MakeRenderPassCreateInfos(const RenderTargetDescription& rtDescription,
                                   const RenderPassConfig& renderPassConfig)
    {
        bool hasDepthStencil = rtDescription.depthStencilDescription.has_value();
        attachmentCount = colorAttachmentCount + (hasDepthStencil ? 1 : 0);

        attachmentDescriptions = std::vector<VkAttachmentDescription>(attachmentCount);
        colorAttachmentReferences = std::vector<VkAttachmentReference>(colorAttachmentCount);

        assert(colorAttachmentCount == renderPassConfig.colors.size() && "color attachment size has to be same size");

        // iterate over color attachments
        for (int i = 0; i < colorAttachmentCount; ++i)
        {
            colorAttachmentReferences[i].attachment = i;
            colorAttachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            const RenderTargetAttachmentDescription& attachmentDesc = rtDescription.colorsDescriptions[i];

            attachmentDescriptions[i].flags = 0;
            attachmentDescriptions[i].format = MapFormat(attachmentDesc.format);
            attachmentDescriptions[i].samples = MapSampleCount(attachmentDesc.multiSampling);
            attachmentDescriptions[i].loadOp = MapAttachmentLoadOp(renderPassConfig.colors[i].loadOp);
            attachmentDescriptions[i].storeOp = MapAttachmentStoreOp(renderPassConfig.colors[i].storeOp);
            attachmentDescriptions[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        if (hasDepthStencil)
        {
            const RenderTargetAttachmentDescription& attachmentDesc = rtDescription.depthStencilDescription.value();
            auto& depthStencilAttachmentDescription = attachmentDescriptions.back();

            depthStencilAttachmentDescription.flags = 0;
            depthStencilAttachmentDescription.format = MapFormat(attachmentDesc.format);
            depthStencilAttachmentDescription.samples = MapSampleCount(attachmentDesc.multiSampling);
            depthStencilAttachmentDescription.loadOp = MapAttachmentLoadOp(renderPassConfig.depthStencil.loadOp);
            depthStencilAttachmentDescription.storeOp = MapAttachmentStoreOp(renderPassConfig.depthStencil.storeOp);

            depthStencilAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthStencilAttachmentReference.attachment = attachmentDescriptions.size() - 1;

            depthStencilAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthStencilAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthStencilAttachmentDescription.stencilLoadOp =
                MapAttachmentLoadOp(renderPassConfig.depthStencil.stencilLoadOp);
            depthStencilAttachmentDescription.stencilStoreOp =
                MapAttachmentStoreOp(renderPassConfig.depthStencil.stencilStoreOp);
        }

        subpassDescriptions = std::vector<VkSubpassDescription>(1);
        auto& subpassDesc = subpassDescriptions[0];
        subpassDesc.flags = 0;
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.inputAttachmentCount = 0;
        subpassDesc.pInputAttachments = VK_NULL_HANDLE;
        subpassDesc.colorAttachmentCount = colorAttachmentReferences.size();
        subpassDesc.pColorAttachments = colorAttachmentReferences.data();
        subpassDesc.pResolveAttachments = VK_NULL_HANDLE;

        subpassDesc.pDepthStencilAttachment = hasDepthStencil ? &depthStencilAttachmentReference : VK_NULL_HANDLE;
        subpassDesc.preserveAttachmentCount = 0;
        subpassDesc.pPreserveAttachments = VK_NULL_HANDLE;
    }
};

VKRenderTarget::VKRenderTarget(VKContext* context,
                               VKAppWindow* window,
                               const RenderTargetDescription& renderTargetDescription)
    : RenderTarget(), memAllocator(context->allocator.Get()), window(window), objectManager(context->objManager.Get()),
      context(context)
{
    SetRenderTargetDescription(renderTargetDescription);
}

VKRenderTarget::~VKRenderTarget()
{
    for (auto& v : renderPasses)
        objectManager->DestroyRenderPass(v.renderPass);

    if (framebuffer_vk != VK_NULL_HANDLE)
        objectManager->DestroyFramebuffer(framebuffer_vk);
}

const std::vector<VkClearValue>& VKRenderTarget::GetClearValues() { return clearValues; }

VkRenderPass VKRenderTarget::RequestRenderPass(const RenderPassConfig& config)
{
    for (auto& pass : renderPasses)
    {
        if (pass.config == config)
            return pass.renderPass;
    }

    // create a new render pass
    VkRenderPass renderPass = VK_NULL_HANDLE;
    RenderTargetCreateHelper helper(&rtDescription, window);
    VkRenderPassCreateInfo renderPassCreateInfo = helper.MakeRenderPassCreateInfo(config);
    objectManager->CreateRenderPass(renderPassCreateInfo, renderPass);

    RenderPassStorage newStore;
    newStore.config = config;
    newStore.renderPass = renderPass;
    renderPasses.push_back(newStore);

    // create a frame buffer if there isn't one. Because the render passes only differ in load store operations, so all
    // the render passes request later should be compatible with each other
    if (framebuffer_vk == VK_NULL_HANDLE)
    {
        // create attachments and framebuffer
        uint32_t width = resolution.width;
        uint32_t height = resolution.height;
        for (const auto& color : rtDescription.colorsDescriptions)
        {
            ImageDescription imageDesc;
            imageDesc.data = nullptr;
            imageDesc.width = width;
            imageDesc.height = height;
            imageDesc.format = color.format;
            imageDesc.multiSampling = color.multiSampling;
            imageDesc.mipLevels = color.mipLevels;
            // TODO: if this render target is a transient target, maybe it doesn't need to be sampled?
            attachments.emplace_back(imageDesc, ImageUsage::ColorAttachment | ImageUsage::Texture);
            attachmentImageViews.push_back(attachments.back().GetDefaultImageView());
        }

        if (rtDescription.HasDepth())
        {
            const auto& dsDescription = rtDescription.depthStencilDescription;
            ImageDescription imageDesc;
            imageDesc.data = nullptr;
            imageDesc.width = width;
            imageDesc.height = height;
            imageDesc.format = dsDescription->format;
            imageDesc.multiSampling = dsDescription->multiSampling;
            imageDesc.mipLevels = dsDescription->mipLevels;

            attachments.emplace_back(imageDesc, ImageUsage::DepthStencilAttachment | ImageUsage::Texture);
            VkImageView imageView = attachments.back().GetDefaultImageView();
            attachmentImageViews.push_back(imageView);
        }

        VkFramebufferCreateInfo frameBufferCreateInfo =
            helper.MakeFrameBufferCreateInfo(renderPass, attachmentImageViews.data(), attachmentImageViews.size());

        objectManager->CreateFramebuffer(frameBufferCreateInfo, framebuffer_vk);
    }

    return renderPass;
}

const VkExtent2D& VKRenderTarget::GetSize() { return resolution; }

void VKRenderTarget::SetRenderTargetDescription(const RenderTargetDescription& renderTargetDescription)
{
    rtDescription = renderTargetDescription;

    clearValues.resize(rtDescription.GetColorAttachmentCount() + (rtDescription.HasDepth() ? 1 : 0));

    uint32_t i = 0;
    for (auto& colorDesc : rtDescription.colorsDescriptions)
    {
        memcpy(&clearValues[i], &colorDesc.clearValue, sizeof(ClearValue));
        i += 1;
    }

    if (rtDescription.HasDepth())
    {
        memcpy(&clearValues[i], &rtDescription.depthStencilDescription->clearValue, sizeof(ClearValue));
    }

    resolution.width = rtDescription.width == 0 ? window->GetDefaultWindowSize().width : rtDescription.width;
    resolution.height = rtDescription.height == 0 ? window->GetDefaultWindowSize().height : rtDescription.height;
}

void VKRenderTarget::TransformAttachmentLayoutIfNeeded(VkCommandBuffer cmd)
{
    uint32_t i = 0;
    uint32_t colorCount = attachments.size() - (rtDescription.HasDepth() ? 1 : 0);
    for (; i < colorCount; ++i)
    {
        attachments[i].TransformLayoutIfNeeded(cmd,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }

    if (rtDescription.HasDepth())
    {
        auto& depth = attachments.back();
        depth.TransformLayoutIfNeeded(cmd,
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }
}

VkFramebuffer VKRenderTarget::GetVkFrameBuffer()
{
    assert(framebuffer_vk != VK_NULL_HANDLE &&
           "frame buffer is not created yet. Maybe you should request a render pass first");
    return framebuffer_vk;
}

VKImage& VKRenderTarget::GetImage(uint32_t index) { return attachments[index]; }
} // namespace Engine::Gfx
