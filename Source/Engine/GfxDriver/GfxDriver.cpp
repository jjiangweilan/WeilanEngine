#include "GfxDriver.hpp"
#include "Vulkan/VKDriver.hpp"
#include <spdlog/spdlog.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace Gfx
{

#if __WIN32__
HMODULE renderdocAPIMod;
#endif

struct GfxDriver::RenderdocModule
{
    HMODULE mod;
};

GfxDriver::GfxDriver()
{
    renderdocModule = std::make_unique<RenderdocModule>();
#if ENGINE_DEV_BUILD
    InitializeRenderDoc();
#endif
}

GfxDriver::~GfxDriver()
{
    if (renderDocAPI != nullptr)
    {
        FreeLibrary(renderdocModule->mod);
    }
}

std::unique_ptr<GfxDriver> GfxDriver::CreateGfxDriver(Backend backend, const CreateInfo& createInfo)
{
    switch (backend)
    {
        case Backend::Vulkan:
            {
                auto gfxDriver = std::make_unique<VKDriver>(createInfo);
                GfxDriver::InstanceInternal() = gfxDriver.get();
                return gfxDriver;
            }
        case Backend::OpenGL: SPDLOG_ERROR("OpenGL backend is not implemented"); break;
        default: break;
    }

    return nullptr;
}

RefPtr<GfxDriver> GfxDriver::Instance()
{
    return InstanceInternal();
}

GfxDriver*& GfxDriver::InstanceInternal()
{
    static GfxDriver* gfxDriver;
    return gfxDriver;
}

std::unique_ptr<Buffer> GfxDriver::CreateBuffer(
    size_t size, BufferUsageFlags usages, bool visibleInCPU, bool gpuWrite, const char* debugName
)
{
    return CreateBuffer(Gfx::Buffer::CreateInfo{usages, size, visibleInCPU, debugName, gpuWrite});
}

void GfxDriver::InitializeRenderDoc()
{
#if __WIN32__
    // At init, on windows

    renderdocModule->mod = LoadLibrary("C:\\Program Files\\RenderDoc\\renderdoc.dll");
    if (renderdocModule->mod)
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(renderdocModule->mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&renderDocAPI);
        ASSERT(ret == 1);
        renderDocAPI->MaskOverlayBits(0, 0);
        spdlog::info("RenderDoc initialized");
    }
    else
        spdlog::info("RenderDoc faield to initialize, can't find renderdoc.dll in C:\\Program Files\\RenderDoc");

#endif
}
} // namespace Gfx
