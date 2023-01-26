#include "GfxDriver.hpp"
#include "Vulkan/VKDriver.hpp"
#include <spdlog/spdlog.h>

namespace Engine::Gfx
{
void GfxDriver::CreateGfxDriver(Backend backend, const CreateInfo& createInfo)
{
    switch (backend)
    {
        case Backend::Vulkan: gfxDriver = MakeUnique<VKDriver>(createInfo); break;
        case Backend::OpenGL: SPDLOG_ERROR("OpenGL backend is not implemented"); break;
        default: break;
    }
}

UniPtr<GfxDriver> GfxDriver::gfxDriver = nullptr;
} // namespace Engine::Gfx
