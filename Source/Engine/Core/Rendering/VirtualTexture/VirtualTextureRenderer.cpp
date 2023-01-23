#include "VirtualTextureRenderer.hpp"
#include "Libs/Image/MipQuadTree.hpp"
#include "ThirdParty/stb/stb_image_write.h"
#include "glm/detail/type_half.hpp"
#include <queue>
#include <set>
#include <thread>

using namespace Libs::Image;
namespace Engine::Rendering
{
struct VTUpdate
{
    struct PageTableUpdate
    {
        glm::ivec2 uv;
        glm::ivec2 pageUV;
        glm::ivec2 offset;
    } pageTable;

    // page that need to be updated to page tex
    bool needPageUpdate;
    struct
    {
        int miplevel;
        int x, y;
    } page;
};
using AnalysisFeedback = std::vector<VTUpdate>;

class VirtualTextureRenderer::FeedbackAnalyzer
{
public:
    void SetParam(Param param, int feedbackWidth, int feedbackHeight)
    {
        this->param = param;
        vt.pageX = std::ceil(this->param.vt->GetWidth() / this->param.pageTexelSize);
        vt.pageY = std::ceil(this->param.vt->GetHeight() / this->param.pageTexelSize);
        vt.totalPages = vt.pageX * vt.pageY;
        indir.width = vt.pageX;
        indir.height = vt.pageY;
        indir.tex = MakeUnique<LinearImage>(indir.width, indir.height, 4, sizeof(float));
        indir.qtree.Resize(indir.width, indir.height);

        cache.pageX = param.pageTexPageNum;
        cache.pageY = param.pageTexPageNum;

        cache.tex = MakeUnique<LinearImage>(param.pageTexelSize * cache.pageX,
                                            param.pageTexelSize * cache.pageY,
                                            4, // PC doesn't support 3 channel 8 bit srgb format
                                            sizeof(unsigned char));
        cache.maxPage = param.pageTexPageNum * param.pageTexPageNum;
        for (auto& node : indir.qtree.GetNodes())
        {
            node.val.lruCache = cache.LRUCache.end();
            node.val.usedPage = cache.usedPages.end();
        }

        cache.freePages.resize(cache.pageX * cache.pageY);
        for (int y = 0; y < cache.pageY; ++y)
        {
            for (int x = 0; x < cache.pageX; ++x)
            {
                int i = x + y * cache.pageX;
                cache.freePages[i].x = x;
                cache.freePages[i].y = y;
            }
        }
    }

    void Analyze(LinearImage* image)
    {
        pageChanged = false;
        feedbackStructs.clear();
        cache.pageRequests.clear();

        for (int i = 0; i < image->GetPixelCount(); ++i)
        {
            struct Feedback
            {
                unsigned short u, v, mip, id;
            };
            Feedback* fb = (Feedback*)(&image->GetData()[4 * i]);
            FeedbackStruct f;
            constexpr int UShort_Max = std::numeric_limits<unsigned short>::max();
            f.uv = {(float)fb->u / UShort_Max, (float)fb->v / UShort_Max};
            f.mip = ((float)fb->mip / UShort_Max) * MAX_LOD;
            f.texID = 0; // not used yet

            auto page = UVToPage(f.uv, f.mip);
            auto id = GetPageID(page, f.mip);

            auto node = indir.qtree.GetNode(id);

            if (node->val.usedPage == cache.usedPages.end())
            {
                cache.pageRequests.insert(node);
            }
        }

        // always keep last mip resident, this can be optimized
        const int lastMipWidth = indir.width / std::pow(2, indir.qtree.GetMaxLevel());
        const int lastMipHeight = indir.height / std::pow(2, indir.qtree.GetMaxLevel());
        for (int y = 0; y < lastMipHeight; ++y)
        {
            for (int x = 0; x < lastMipWidth; ++x)
            {
                auto id = GetPageID({x, y}, indir.qtree.GetMaxLevel());
                auto node = indir.qtree.GetNode(id);

                if (node->val.usedPage == cache.usedPages.end())
                {
                    cache.pageRequests.insert(node);
                }
            }
        }

        bool pageChanded = CachePagesIntoCPUMemory();
        if (pageChanded)
        {
            UpdateIndirTex();
        }
    }

    bool GetFinalTextures(Libs::Image::LinearImage*& cache, Libs::Image::LinearImage*& indirMap)
    {
        indirMap = indir.tex.Get();
        cache = this->cache.tex.Get();

        return pageChanged;
    }

private:
    struct FeedbackStruct
    {
        glm::vec2 uv;
        float mip;
        float texID;

        glm::ivec2 pageUVInVT;
        glm::vec2 pageUVInCache;
    };

    struct QuadTreeNodeData
    {
        std::list<int>::iterator lruCache;
        std::list<glm::ivec2>::iterator usedPage;
    };

    struct LowerMiap
    {
        bool operator()(MipQuadTree<QuadTreeNodeData>::Node* const& l,
                        MipQuadTree<QuadTreeNodeData>::Node* const& r) const
        {
            return l->level < r->level && l->id != r->id;
        }
    };

    struct CacheTex
    {
        int pageX;
        int pageY;

        int width;
        int height;

        int maxPage;

        // index: pageID
        std::set<typename MipQuadTree<QuadTreeNodeData>::Node*, LowerMiap> pageRequests;
        std::list<int> LRUCache;
        // a quad tree represent the cache image
        UniPtr<LinearImage> tex;

        std::vector<glm::ivec2> freePages;
        std::list<glm::ivec2> usedPages;
    } cache;

    struct IndirectionTex
    {
        int width;
        int height;

        struct PixelType
        {
            float scaleX, scaleY, biasX, biasY;
        };

        MipQuadTree<QuadTreeNodeData> qtree;
        UniPtr<LinearImage> tex;
    } indir;

    struct VTTex
    {
        // all these data only describe mip 0
        int pageX;
        int pageY;
        int totalPages;
    } vt;

    const int MAX_LOD = 6; // defined in shader, used in flatPageUV, can't be greater than 10

    void UpdateIndirTex()
    {
        struct ScaleBias
        {
            float scaleX, scaleY, biasX, biasY;
        };
        const int pageCountX = param.vt->GetWidth() / param.pageTexelSize;
        const int pageCountY = param.vt->GetHeight() / param.pageTexelSize;
        ScaleBias* ptr = (ScaleBias*)indir.tex->GetData();
        for (int y = 0; y < pageCountY; ++y)
        {
            for (int x = 0; x < pageCountX; ++x)
            {
                ScaleBias* offset = ptr + y * pageCountX + x;
                for (int mip = 0; mip < MAX_LOD; ++mip)
                {
                    float powMip = std::pow(2, mip);
                    int mipPageX = x >> mip;
                    int mipPageY = y >> mip;
                    int mipTotalPageX = pageCountX >> mip;
                    int mipTotalPageY = pageCountY >> mip;
                    int id = GetPageID({mipPageX, mipPageY}, mip);
                    auto node = indir.qtree.GetNode(id);
                    if (node->val.usedPage != cache.usedPages.end())
                    {
                        float bx = node->val.usedPage->x;
                        float by = node->val.usedPage->y;

                        int s0 = std::pow(2, mip);
                        offset->scaleX = (float)param.vt->GetWidth() / s0 / cache.tex->GetWidth();
                        float n0 = bx / cache.pageX;
                        float n1 = (float)mipPageX / mipTotalPageX;
                        offset->biasX = n0 - n1 * offset->scaleX;

                        offset->scaleY = (float)param.vt->GetHeight() / s0 / cache.tex->GetHeight();
                        offset->biasY = (by / (float)cache.pageY - mipPageY / (float)mipTotalPageY * offset->scaleY);
                        break;
                    }
                }
            }
        }
    }

    bool CachePagesIntoCPUMemory()
    {
        struct ScheduledUpaload
        {
            int id;
            int cacheX, cacheY;
        };

        std::vector<ScheduledUpaload> scheduledUpload;
        for (auto requestIter = cache.pageRequests.begin(); requestIter != cache.pageRequests.end(); ++requestIter)
        {
            auto node = *requestIter;

            // if the page is already cached, we remove the current cache so that we can promote it to front
            if (node->val.lruCache != cache.LRUCache.end())
            {
                cache.LRUCache.erase(node->val.lruCache);
            }
            // if the cache is full, pop the least used one
            else if (cache.usedPages.size() >= cache.maxPage)
            {
                int id = cache.LRUCache.back();
                auto backNode = indir.qtree.GetNode(id);
                backNode->val.lruCache = cache.LRUCache.end();
                cache.LRUCache.pop_back();

                cache.freePages.push_back(*backNode->val.usedPage);
                cache.usedPages.erase(backNode->val.usedPage);
                backNode->val.usedPage = cache.usedPages.end();
            }

            cache.LRUCache.push_front(node->id);

            if (node->val.lruCache == cache.LRUCache.end())
            {
                glm::ivec2 freePage = cache.freePages.back();
                cache.freePages.pop_back();
                cache.usedPages.push_front({freePage});
                node->val.usedPage = cache.usedPages.begin();

                scheduledUpload.push_back({node->id, freePage.x, freePage.y});
            }

            node->val.lruCache = cache.LRUCache.begin();
        }
        struct RGB
        {
            unsigned char r, g, b, a;
        };
        RGB* ptr = (RGB*)cache.tex->GetData();
        int cacheTexPixelWidth = param.pageTexelSize * cache.pageX;
        const int element = cache.tex->GetChannel();
        for (ScheduledUpaload i : scheduledUpload)
        {
            auto node = indir.qtree.GetNode(i.id);
            if (node)
            {
                pageChanged = true;
                auto vtPage = param.vt->Read(node->x, node->y, node->level, element);
                glm::ivec2 cachePage = *node->val.usedPage;

                RGB* ori =
                    ptr + cachePage.y * param.pageTexelSize * cacheTexPixelWidth + cachePage.x * param.pageTexelSize;

                for (int y = 0; y < param.pageTexelSize; ++y)
                {
                    memcpy(ori + y * cacheTexPixelWidth,
                           (RGB*)vtPage.GetData() + y * param.pageTexelSize,
                           param.pageTexelSize * element);
                }
            }
            else
            {
                SPDLOG_ERROR("VirtualTextureRenderer-node is null, index: {}", i.id);
            }
        }

        const Uint8* state = SDL_GetKeyboardState(nullptr);

        if (state[SDL_SCANCODE_Y])
        {
            stbi_write_jpg("test.jpg",
                           cache.tex->GetWidth(),
                           cache.tex->GetHeight(),
                           cache.tex->GetChannel(),
                           cache.tex->GetData(),
                           90);
        }

        return pageChanged;
    }

    glm::ivec2 UVToPage(glm::vec2 uv, int mip)
    {
        float scale = std::pow(2, mip);
        glm::vec2 vtExtent = {param.vt->GetWidth(), param.vt->GetHeight()};
        glm::vec2 vtMipExtent = vtExtent / scale;
        glm::ivec2 pageUV = glm::floor(uv * vtMipExtent / (float)param.pageTexelSize);

        return pageUV;
    }

    int GetPageID(glm::ivec2 page, int mip)
    {
        int offset = 0;
        for (int i = 0; i < mip; ++i)
        {
            offset += vt.totalPages / std::pow(4, i);
        }

        return offset + page.y * vt.pageX / std::pow(2, mip) + page.x;
    }

    Param param;
    bool pageChanged = false;

    std::vector<FeedbackStruct> feedbackStructs;
};

VirtualTextureRenderer::~VirtualTextureRenderer() = default;

bool VirtualTextureRenderer::GetFinalTextures(LinearImage*& cache, LinearImage*& indirMap)
{
    return feedbackAnalyzer->GetFinalTextures(cache, indirMap);
}

void VirtualTextureRenderer::SetVT(RefPtr<VirtualTexture> vt,
                                   uint32_t frameTexWidth,
                                   uint32_t frameTexheight,
                                   uint32_t feedbackTexScale)
{
    param.vt = vt.Get();

    // resource init
    feedbackPassData.shader = AssetDatabase::Instance()->GetShader("VirtualTexture/FeedbackTex");
    feedbackPassData.resource = GetGfxDriver()->CreateShaderResource(feedbackPassData.shader->GetShaderProgram(),
                                                                     Gfx::ShaderResourceFrequency::Material);

    float feedbackShaderParam[3] = {(float)vt->GetWidth(), (float)vt->GetHeight(), -(float)std::log2(feedbackTexScale)};
    Internal::GfxResourceTransfer::BufferTransferRequest bufferTransferRequest{.data = &feedbackShaderParam,
                                                                               .bufOffset = 0,
                                                                               .size = sizeof(feedbackShaderParam)};
    Gfx::ShaderResource::BufferMemberInfoMap bufMemInfoMap;
    Internal::GetGfxResourceTransfer()->Transfer(
        feedbackPassData.resource->GetBuffer("FeedbackTexParam", bufMemInfoMap),
        bufferTransferRequest);

    // build graph
    g.feedbackTex = g.feedbackTexGraph.AddNode<RGraph::ImageNode>();
    g.feedbackTex->width = frameTexWidth / feedbackTexScale;
    g.feedbackTex->height = frameTexheight / feedbackTexScale;
    g.feedbackTex->format = Gfx::ImageFormat::R16G16B16A16_UNorm;
    g.feedbackTex->SetName("VirtualTextureRenderer-feedbackTex");

    g.feedbackDepthTex = g.feedbackTexGraph.AddNode<RGraph::ImageNode>();
    g.feedbackDepthTex->width = frameTexWidth / feedbackTexScale;
    g.feedbackDepthTex->height = frameTexheight / feedbackTexScale;
    g.feedbackDepthTex->format = Gfx::ImageFormat::D16_UNorm;
    g.feedbackDepthTex->SetName("VirtualTextureRenderer-feedbackDepthTex");

    g.feedbackPass = g.feedbackTexGraph.AddNode<RGraph::RenderPassNode>();
    g.feedbackPass->GetClearValues()[0].color = {{0, 0, 0, 0}};
    g.feedbackPass->GetClearValues().back().depthStencil = {1, 0};
    g.feedbackPass->GetPortColorIn(0)->Connect(g.feedbackTex->GetPortOutput());
    g.feedbackPass->GetPortDepthIn()->Connect(g.feedbackDepthTex->GetPortOutput());

    RGraph::DrawData drawDataOverride;
    drawDataOverride.shader = feedbackPassData.shader->GetShaderProgram().Get();
    drawDataOverride.shaderConfig = &feedbackPassData.shader->GetDefaultShaderConfig();
    drawDataOverride.shaderResource = feedbackPassData.resource.Get();
    drawDataOverride.scissor = Rect2D{{0, 0}, {g.feedbackTex->width, g.feedbackTex->height}};
    g.feedbackPass->OverrideDrawData(drawDataOverride);

    g.readbackBuf = g.feedbackTexGraph.AddNode<RGraph::BufferNode>();
    g.readbackBuf->props.size =
        g.feedbackTex->width * g.feedbackTex->height *
        Gfx::Utils::MapImageFormatToByteSize(Gfx::ImageFormat::R16G16B16A16_UNorm); // R16G16B16A16_SFloat
    g.readbackBuf->props.debugName = "VirtualTextureRenderer-feadbackBuf";
    g.readbackBuf->props.visibleInCPU = true;

    auto readbackNode = g.feedbackTexGraph.AddNode<RGraph::MemoryTransferNode>();
    readbackNode->in.srcImage->Connect(g.feedbackPass->GetPortColorOut(0));
    readbackNode->in.dstBuffer->Connect(g.readbackBuf->out.buffer);
    g.feedbackTexGraph.Compile();

    feedbackTex = MakeUnique<LinearImage>(g.feedbackTex->width, g.feedbackTex->height, 4, sizeof(short));
    feedbackAnalyzer = MakeUnique<FeedbackAnalyzer>();
    feedbackAnalyzer->SetParam(param, feedbackTex->GetWidth(), feedbackTex->GetHeight());
}

void VirtualTextureRenderer::BuildMainGraph(RGraph::RenderGraph& graph, RGraph::Port*& pageTex, RGraph::Port*& indirTex)
{
    auto pageTexTransfer = graph.AddNode<RGraph::MemoryTransferNode>();
    auto indirTexTransfer = graph.AddNode<RGraph::MemoryTransferNode>();
}

void VirtualTextureRenderer::RunAnalysis()
{
    RGraph::ResourceStateTrack stateTrack;
    cmdPool->ResetCommandPool();
    feedbackCmdBuf->Begin();
    g.feedbackTexGraph.Execute(feedbackCmdBuf.Get(), stateTrack);
    feedbackCmdBuf->End();

    RefPtr<CommandBuffer> cmdBufs[] = {feedbackCmdBuf};
    RefPtr<Gfx::Semaphore> waitSemaphores[] = {feedbackGenerationBeginSemaphore};
    Gfx::PipelineStageFlags pipelineStages[] = {Gfx::PipelineStage::Transfer};
    GetGfxDriver()->QueueSubmit(queue, cmdBufs, waitSemaphores, pipelineStages, {}, feedbackTexPassFence);

    // TODO: naive approach, need to be optimized
    GetGfxDriver()->WaitForFence({feedbackTexPassFence}, true, -1);
    feedbackTexPassFence->Reset();

    // analyze
    auto readbackBuf = (Gfx::Buffer*)g.readbackBuf->out.buffer->GetResourceVal();
    memcpy(feedbackTex->GetData(), readbackBuf->GetCPUVisibleAddress(), readbackBuf->GetSize());

    feedbackAnalyzer->Analyze(feedbackTex.Get());
}

} // namespace Engine::Rendering
