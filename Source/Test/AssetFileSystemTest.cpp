#include "Engine/Library/UUID.hpp"
#include "Engine/Runtime/System/AssetDatabase/Private/AssetData.hpp"
#include "Engine/Runtime/System/AssetDatabase/Private/AssetFileSystem.hpp"
#include "Engine/Runtime/System/EngineConfig.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace
{
class AssetFileSystemMoveTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        originalProjectRoot = EngineConfig::GetProjectRoot();
        projectRoot = std::filesystem::temp_directory_path() /
                      ("WeilanEngine_AssetFileSystemMoveTest_" + UUID().ToString());
        std::filesystem::create_directories(projectRoot / "Assets");
        EngineConfig::SetProjectRoot(projectRoot);
        fileSystem.Init(projectRoot);
    }

    void TearDown() override
    {
        EngineConfig::SetProjectRoot(originalProjectRoot);
        std::error_code error;
        std::filesystem::remove_all(projectRoot, error);
    }

    void WriteAsset(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path path = projectRoot / "Assets" / relativePath;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << "asset";
    }

    std::filesystem::path originalProjectRoot;
    std::filesystem::path projectRoot;
    AssetFileSystem fileSystem;
};

TEST_F(AssetFileSystemMoveTest, MovesFolderAndRemapsNestedAssetData)
{
    WriteAsset("Source/Nested/asset.bin");
    std::filesystem::create_directories(projectRoot / "Assets/Target");
    AssetData assetData(AssetPath("Source/Nested/asset.bin"), projectRoot);
    fileSystem.Add(&assetData);

    std::string error;
    ASSERT_TRUE(fileSystem.Move({AssetPath("Source")}, AssetPath("Target"), error)) << error;

    EXPECT_TRUE(std::filesystem::exists(projectRoot / "Assets/Target/Source/Nested/asset.bin"));
    EXPECT_FALSE(std::filesystem::exists(projectRoot / "Assets/Source"));
    EXPECT_EQ(assetData.GetAssetPath(), AssetPath("Target/Source/Nested/asset.bin"));
    EXPECT_EQ(fileSystem.GetAssetData(AssetPath("Target/Source/Nested/asset.bin")), &assetData);
    EXPECT_EQ(fileSystem.GetAssetData(AssetPath("Source/Nested/asset.bin")), nullptr);
}

TEST_F(AssetFileSystemMoveTest, MovesMultipleFilesAndFoldersAsOneBatch)
{
    WriteAsset("Folder/inside.bin");
    WriteAsset("loose.bin");
    std::filesystem::create_directories(projectRoot / "Assets/Target");

    std::string error;
    ASSERT_TRUE(
        fileSystem.Move({AssetPath("Folder"), AssetPath("loose.bin")}, AssetPath("Target"), error)
    ) << error;

    EXPECT_TRUE(std::filesystem::exists(projectRoot / "Assets/Target/Folder/inside.bin"));
    EXPECT_TRUE(std::filesystem::exists(projectRoot / "Assets/Target/loose.bin"));
}

TEST_F(AssetFileSystemMoveTest, CollapsesNestedSourcesIntoTheirSelectedParent)
{
    WriteAsset("Folder/inside.bin");
    std::filesystem::create_directories(projectRoot / "Assets/Target");

    std::string error;
    ASSERT_TRUE(
        fileSystem.Move({AssetPath("Folder/inside.bin"), AssetPath("Folder")}, AssetPath("Target"), error)
    ) << error;

    EXPECT_TRUE(std::filesystem::exists(projectRoot / "Assets/Target/Folder/inside.bin"));
}

TEST_F(AssetFileSystemMoveTest, RejectsCollisionWithoutMovingAnySource)
{
    WriteAsset("One/shared.bin");
    WriteAsset("Two/shared.bin");
    std::filesystem::create_directories(projectRoot / "Assets/Target");

    std::string error;
    EXPECT_FALSE(
        fileSystem.Move(
            {AssetPath("One/shared.bin"), AssetPath("Two/shared.bin")},
            AssetPath("Target"),
            error
        )
    );

    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(std::filesystem::exists(projectRoot / "Assets/One/shared.bin"));
    EXPECT_TRUE(std::filesystem::exists(projectRoot / "Assets/Two/shared.bin"));
    EXPECT_FALSE(std::filesystem::exists(projectRoot / "Assets/Target/shared.bin"));
}

TEST_F(AssetFileSystemMoveTest, RejectsFolderMovedIntoItsDescendant)
{
    WriteAsset("Parent/Child/inside.bin");

    std::string error;
    EXPECT_FALSE(fileSystem.Move({AssetPath("Parent")}, AssetPath("Parent/Child"), error));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(std::filesystem::exists(projectRoot / "Assets/Parent/Child/inside.bin"));
}
} // namespace
