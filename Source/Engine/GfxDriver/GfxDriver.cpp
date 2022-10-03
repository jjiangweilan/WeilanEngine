#include "GfxDriver.hpp"
#include "Vulkan/VKDriver.hpp"
#include <spdlog/spdlog.h>

namespace Engine::Gfx
{
    void GfxDriver::CreateGfxDriver(Backend backend)
    {
        switch (backend) {
            case Backend::Vulkan:
                gfxDriver = MakeUnique<VKDriver>();
                break;
            case Backend::OpenGL:
                SPDLOG_ERROR("OpenGL backend is not implemented");
                break;
            default:
                break;
        }
    }

    UniPtr<GfxDriver> GfxDriver::gfxDriver = nullptr;
}
