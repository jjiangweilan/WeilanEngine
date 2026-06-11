#include "Engine/Runtime/System/AssetDatabase/Importers/TextureImporter.hpp"
#include "Engine/Runtime/System/AssetDatabase/Private/ImportDatabase.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

TEST(TextureImporterTest, CorruptPngDoesNotRegisterImportStateOrArtifact)
{
    const std::filesystem::path testRoot =
        std::filesystem::temp_directory_path() / ("WeilanEngine_TextureImporterTest_" + UUID().ToString());
    const std::filesystem::path sourcePath = testRoot / "corrupt.png";
    const std::filesystem::path importRoot = testRoot / "Imported";
    const UUID assetUUID;

    std::filesystem::create_directories(testRoot);
    {
        std::ofstream source(sourcePath, std::ios::binary);
        const char invalidPng[] = "not a png";
        source.write(invalidPng, sizeof(invalidPng));
    }

    {
        ImportDatabase importDatabase;
        importDatabase.Init(importRoot);

        TextureImporter importer;
        importer.Setup(importDatabase, assetUUID, sourcePath, nlohmann::json::object());
        EXPECT_NO_THROW(importer.Import());

        ImportDatabase::ImportState state;
        EXPECT_FALSE(importDatabase.TryGetImportState(assetUUID.ToString(), state));
        EXPECT_TRUE(importDatabase.ListArtifacts(assetUUID).empty());
    }

    std::error_code error;
    std::filesystem::remove_all(testRoot, error);
}

TEST(TextureImporterTest, CorruptReimportPreservesPreviousStateAndArtifact)
{
    const std::filesystem::path testRoot =
        std::filesystem::temp_directory_path() / ("WeilanEngine_TextureReimportTest_" + UUID().ToString());
    const std::filesystem::path sourcePath = testRoot / "corrupt.png";
    const std::filesystem::path importRoot = testRoot / "Imported";
    const std::filesystem::path previousArtifact = "previous.ktx";
    const UUID assetUUID;
    const ImportDatabase::ImportState previousState{11, 22, 33};

    std::filesystem::create_directories(testRoot);
    {
        std::ofstream source(sourcePath, std::ios::binary);
        source << "not a png";
    }

    {
        ImportDatabase importDatabase;
        importDatabase.Init(importRoot);
        {
            std::ofstream artifact(importRoot / previousArtifact, std::ios::binary);
            artifact << "previous artifact";
        }
        importDatabase.UpsertImportState(assetUUID.ToString(), previousState);
        importDatabase.ReplaceArtifact(assetUUID.ToString(), AssetArtifacts::Kind::Texture, previousArtifact);

        TextureImporter importer;
        importer.Setup(importDatabase, assetUUID, sourcePath, nlohmann::json::object());
        EXPECT_NO_THROW(importer.Import());

        ImportDatabase::ImportState actualState;
        ASSERT_TRUE(importDatabase.TryGetImportState(assetUUID.ToString(), actualState));
        EXPECT_EQ(actualState.sourceWriteTime, previousState.sourceWriteTime);
        EXPECT_EQ(actualState.metaHash, previousState.metaHash);
        EXPECT_EQ(actualState.contentHash, previousState.contentHash);

        std::filesystem::path actualArtifact;
        ASSERT_TRUE(importDatabase.TryGetArtifactPath(
            assetUUID.ToString(),
            AssetArtifacts::Kind::Texture,
            actualArtifact
        ));
        EXPECT_EQ(actualArtifact, previousArtifact);
        EXPECT_TRUE(std::filesystem::exists(importRoot / previousArtifact));
    }

    std::error_code error;
    std::filesystem::remove_all(testRoot, error);
}
