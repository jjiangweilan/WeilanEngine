#pragma once
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include "GfxDriver/GfxDriver.hpp"
#include "Core/AssetDatabase/Importers/ShaderImporter.hpp"
#include "Code/UUID.hpp"

TEST(Importer, ShaderImport)
{
    Engine::Gfx::GfxDriver::CreateGfxDriver(Engine::Gfx::Backend::Vulkan);

    Engine::Internal::ShaderImporter importer;
    auto currPath = std::filesystem::path(__FILE__);
    auto root = currPath.parent_path();
    auto shadPath = root / "Resources/simple.shad";

    Engine::ReferenceResolver dummy;
    Engine::UUID uuid(std::string("3c3e276d-cd64-4c11-9031-10dc146929f8"));
    importer.Import(shadPath, root, {}, uuid, {});
    auto shader = importer.Load(root, dummy, uuid);
    EXPECT_NE(shader, nullptr);

    bool needReimport = importer.NeedReimport(shadPath, root, uuid);
    EXPECT_EQ(needReimport, false);

    shader = nullptr;
    Engine::Gfx::GfxDriver::DestroyGfxDriver();
}
