#include "GfxDriver.hpp"
#include "Vulkan/VKDriver.hpp"
#include <spdlog/spdlog.h>

namespace Engine::Gfx
{
std::unique_ptr<GfxDriver> GfxDriver::CreateGfxDriver(Backend backend, const CreateInfo& createInfo)
{
    switch (backend)
    {
        case Backend::Vulkan:
            {
                auto gfxDriver = std::make_unique<VKDriver>(createInfo);
                GfxDriver::gfxDriver = gfxDriver.get();
                return gfxDriver;
            }
        case Backend::OpenGL: SPDLOG_ERROR("OpenGL backend is not implemented"); break;
        default: break;
    }

    return nullptr;
}

GfxDriver* GfxDriver::gfxDriver = nullptr;
} // namespace Engine::Gfx
