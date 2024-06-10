#include "GfxDriver.hpp"
#include "Vulkan/VKDriver.hpp"
#include <spdlog/spdlog.h>

namespace Gfx
{
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
} // namespace Gfx
