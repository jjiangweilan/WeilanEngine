#include <gtest/gtest.h>
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include "Engine/Runtime/System/EngineConfig.hpp"
#include <filesystem>

class AssetPathTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Mock project root and engine root for testing
        std::filesystem::path currentPath = std::filesystem::current_path();
        EngineConfig::SetProjectRoot(currentPath / "TestProject");
    }
};

TEST_F(AssetPathTest, Normalization) {
    AssetPath p1("Textures\\Grass.png");
    EXPECT_EQ(p1.string(), "textures/grass.png");
    EXPECT_FALSE(p1.IsInternal());

    AssetPath p2("_ENGINE_INTERNAL/Shaders/Lit.shad/");
    EXPECT_EQ(p2.string(), "_engine_internal/shaders/lit.shad");
    EXPECT_TRUE(p2.IsInternal());
}

TEST_F(AssetPathTest, PathComponents) {
    AssetPath p("textures/nature/grass.png");
    EXPECT_EQ(p.GetExtension(), ".png");
    EXPECT_EQ(p.GetFileName(), "grass.png");
    EXPECT_EQ(p.GetFileNameWithoutExtension(), "grass");
    EXPECT_EQ(p.GetParentPath().string(), "textures/nature");

    AssetPath rootP("root_file.txt");
    EXPECT_EQ(rootP.GetParentPath().string(), "");
}

TEST_F(AssetPathTest, AbsolutePathResolution) {
    std::filesystem::path projectAssets = EngineConfig::GetProjectAssetsRoot();
    std::filesystem::path engineAssets = EngineConfig::GetEngineAssetsRoot();

    AssetPath p1("textures/grass.png");
    EXPECT_EQ(p1.ToAbsolutePath(), projectAssets / "textures/grass.png");

    AssetPath p2("_engine_internal/shaders/lit.shad");
    EXPECT_EQ(p2.ToAbsolutePath(), engineAssets / "shaders/lit.shad");

    // Construct from absolute path
    AssetPath p3(projectAssets / "models/cube.fbx");
    EXPECT_EQ(p3.string(), "models/cube.fbx");
    EXPECT_FALSE(p3.IsInternal());

    AssetPath p4(engineAssets / "editor/icons/folder.png");
    EXPECT_EQ(p4.string(), "_engine_internal/editor/icons/folder.png");
    EXPECT_TRUE(p4.IsInternal());

    AssetPath p5("C:/Windows/System32/kernel32.dll");
    EXPECT_TRUE(p5.empty());
}

TEST_F(AssetPathTest, ImplicitConversion) {
    AssetPath p("textures/grass.png");
    std::filesystem::path stdP = p;
    EXPECT_EQ(stdP, EngineConfig::GetProjectAssetsRoot() / "textures/grass.png");
}

TEST_F(AssetPathTest, InternalPrefixHandling) {
    // Constructing with prefix already present should not double it
    AssetPath p("_engine_internal/shaders/lit.shad");
    EXPECT_EQ(p.string(), "_engine_internal/shaders/lit.shad");
    EXPECT_TRUE(p.IsInternal());

    // Path join with prefix already present in right-hand side
    AssetPath base("_engine_internal");
    AssetPath sub("shaders/lit.shad");
    AssetPath joined = base / sub;
    EXPECT_EQ(joined.string(), "_engine_internal/shaders/lit.shad");

    // Joining base with a path that already has prefix (this was the bug)
    AssetPath alreadyInternal("_engine_internal/shaders/lit.shad");
    AssetPath doubled = base / alreadyInternal;
    // Normalize should now deduplicate this!
    EXPECT_EQ(doubled.string(), "_engine_internal/shaders/lit.shad"); 
}
