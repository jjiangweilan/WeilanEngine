#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "GfxDriver/Vulkan/Exp/VKShaderResource.hpp"
#include "GfxDriver/Vulkan/VKDriver.hpp"
#include "GfxDriver/Vulkan/VKFactory.hpp"
#include "GfxDriver/Vulkan/Exp/VKShaderModule.hpp"
#include "GfxDriver/Vulkan/Exp/VKBuffer.hpp"

using namespace Engine;

namespace
{
class VKDriver : public ::testing::Test
{
protected:

    VKDriver()
    {
        spdlog::set_level(spdlog::level::off);
        driver = MakeUnique<Engine::Gfx::VKDriver>();
        driver->InitGfxFactory();
        factory = static_cast<Gfx::VKFactory*>(Gfx::GfxFactory::Instance().Get());
        shaderLoader = (Gfx::VKShaderLoader*)driver->GetShaderLoader().Get();
    }

    ~VKDriver() override {}

    UniPtr<Engine::Gfx::VKDriver> driver;
    RefPtr<Engine::Gfx::VKFactory> factory;
    RefPtr<Engine::Gfx::VKShaderLoader> shaderLoader;
};

TEST_F(VKDriver, VKBuffer)
{
    auto buf = factory->CreateBuffer(32, Gfx::BufferUsage::Uniform, false);
    auto vkBuf = (Gfx::Exp::VKBuffer*)buf.Get();
    ASSERT_NE(vkBuf->GetVKBuffer(), VK_NULL_HANDLE);
}
}
