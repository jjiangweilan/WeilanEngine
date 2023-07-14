#pragma once
#include "AssetDatabase/Importers.hpp"
#include "Rendering/BuiltinShader.hpp"
#include <gtest/gtest.h>

TEST(Importers, GLB)
{
    auto gfxDriver = Engine::Gfx::GfxDriver::CreateGfxDriver(Engine::Gfx::Backend::Vulkan, {.windowSize = {960, 540}});
    auto builtinShaders = Engine::Rendering::BuiltinShader::Init();
    auto model2 = Engine::Importers::GLB(
        "Source/Test/Resources/Cube.glb",
        builtinShaders->GetShader(Engine::Rendering::BuiltinShader::ShaderName::StandardPBR)
    );
}
