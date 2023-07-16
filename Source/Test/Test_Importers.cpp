#pragma once
#include "AssetDatabase/Importers.hpp"
#include "Rendering/BuiltinShader.hpp"
#include "WeilanEngine.hpp"
#include <gtest/gtest.h>

TEST(Importers, GLB)
{
    auto engine = std::make_unique<Engine::WeilanEngie>();

    engine->Init({});
    Engine::Shader* shader = engine->shaders->Add("StandardPBR", "Assets/Shaders/Game/StandardPBR.shad");

    auto model2 = Engine::Importers::GLB("Source/Test/Resources/Cube.glb", shader);

    engine->Loop();
}
