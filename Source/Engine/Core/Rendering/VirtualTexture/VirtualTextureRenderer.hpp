// #pragma once
// #include "Core/Rendering/RenderGraph/Nodes/BufferNode.hpp"
// #include "Core/Rendering/RenderGraph/Nodes/MemoryTransferNode.hpp"
// #include "Core/Rendering/RenderGraph/RenderGraph.hpp"
// #include "GfxDriver/Buffer.hpp"
// #include "GfxDriver/GfxDriver.hpp"
// #include "Libs/Image/LinearImage.hpp"
// #include "Rendering/GfxResourceTransfer.hpp"
// #include "VirtualTexture.hpp"
//
// #if defined(_WIN32) || defined(_WIN64)
// #undef CreateSemaphore
// #endif
//
// namespace Rendering
//{
//
///**
// * Feedback tex generation:
// * vt renderer uses the same drawlist as the
// *
// * capacity:
// * page size: 128x128
// * number of virtual texture: 1
// * multithreaded: no
// * maximum virtual texture size: 32k
// */
// class VirtualTextureRenderer
//{
// public:
//    VirtualTextureRenderer()
//    {
//        queue = GetGfxDriver()->GetQueue(QueueType::Main);
//        Gfx::CommandPool::CreateInfo createInfo{.queueFamilyIndex = queue->GetFamilyIndex()};
//        cmdPool = GetGfxDriver()->CreateCommandPool(createInfo);
//        feedbackCmdBuf = std::move(cmdPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0]);
//        feedbackTexPassFence = GetGfxDriver()->CreateFence({false});
//        feedbackGenerationBeginSemaphore = GetGfxDriver()->CreateSemaphore({false});
//    }
//
//    ~VirtualTextureRenderer();
//
//    RefPtr<Gfx::Semaphore> GetFeedbackGenerationBeginSemaphore() { return feedbackGenerationBeginSemaphore; };
//    void BuildMainGraph(RGraph::RenderGraph& graph, RGraph::Port*& pageTex, RGraph::Port*& indirTex);
//    void SetVT(RefPtr<VirtualTexture> vt, uint32_t frameTexWidth, uint32_t frameTexheight, uint32_t feedbackTexScale);
//    void SetVTObjectDrawList(RGraph::DrawList& drawList) { g.feedbackPass->SetDrawList(drawList); }
//
//    void RunAnalysis();
//
//    // get final cache and indirMap
//    // return true if they are updated
//    bool GetFinalTextures(Libs::Image::LinearImage*& cache, Libs::Image::LinearImage*& indirMap);
//
// private:
//    struct Param
//    {
//        VirtualTexture* vt;
//        int pageTexelSize = 128;
//        int pageTexPageNum = 32; // 4096 * 4096
//    } param;
//
//    class FeedbackAnalyzer;
//    UniPtr<FeedbackAnalyzer> feedbackAnalyzer;
//    UniPtr<Libs::Image::LinearImage> feedbackTex;
//
//    UniPtr<Gfx::CommandPool> cmdPool;
//    UniPtr<Gfx::CommandBuffer> feedbackCmdBuf;
//    UniPtr<Gfx::CommandBuffer> readbackCmdBuf;
//    UniPtr<Gfx::Semaphore> feedbackGenerationBeginSemaphore;
//    RefPtr<CommandQueue> queue;
//    UniPtr<Gfx::Fence> feedbackTexPassFence;
//
//    std::unique_ptr<std::thread> analysisThread;
//
//    struct
//    {
//        RGraph::RenderGraph feedbackTexGraph;
//        // rgba format rgba8888: rg(vir tex page coord), b(mipmap level), a(virture texture id)
//        RGraph::ImageNode* feedbackTex;
//        RGraph::ImageNode* feedbackDepthTex;
//        RGraph::RenderPassNode* feedbackPass;
//        RGraph::BufferNode* readbackBuf;
//    } g;
//
//    struct
//    {
//        RefPtr<Shader> shader;
//        UniPtr<Gfx::ShaderResource> resource;
//    } feedbackPassData;
//};
//} // namespace Rendering
