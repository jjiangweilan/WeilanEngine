#include "Engine/Runtime/System/AssetDatabase/Importers/TextureImporter.hpp"
#include "Engine/Runtime/System/AssetDatabase/Private/ImportDatabase.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ktx.h>
#include <vulkan/vulkan.h>
#include <ktxvulkan.h>

namespace
{
struct ImportedPpm
{
    khr_df_transfer_e transfer = KHR_DF_TRANSFER_UNSPECIFIED;
    nlohmann::json meta;
};

ImportedPpm ImportPpm(const nlohmann::json& meta, const std::string& fileName)
{
    const std::filesystem::path testRoot =
        std::filesystem::temp_directory_path() / ("WeilanEngine_TextureColorSpaceTest_" + UUID().ToString());
    const std::filesystem::path sourcePath = testRoot / fileName;
    const std::filesystem::path importRoot = testRoot / "Imported";
    const UUID assetUUID;
    ImportedPpm imported;

    std::filesystem::create_directories(testRoot);
    {
        std::ofstream source(sourcePath, std::ios::binary);
        source << "P6\n1 1\n255\n";
        const uint8_t pixel[] = {128, 64, 32};
        source.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
    }

    {
        ImportDatabase importDatabase;
        importDatabase.Init(importRoot);

        TextureImporter importer;
        importer.Setup(importDatabase, assetUUID, sourcePath, meta);
        importer.Import();
        imported.meta = importer.GetMeta();

        std::filesystem::path artifactPath;
        EXPECT_TRUE(importDatabase.TryGetArtifactPath(assetUUID.ToString(), AssetArtifacts::Kind::Texture, artifactPath));
        ktxTexture2* texture = nullptr;
        EXPECT_EQ(
            ktxTexture2_CreateFromNamedFile(
                (importRoot / artifactPath).string().c_str(),
                KTX_TEXTURE_CREATE_NO_FLAGS,
                &texture
            ),
            KTX_SUCCESS
        );
        if (texture)
        {
            imported.transfer = ktxTexture2_GetOETF_e(texture);
            ktxTexture_Destroy(ktxTexture(texture));
        }
    }

    std::error_code error;
    std::filesystem::remove_all(testRoot, error);
    return imported;
}
} // namespace

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

TEST(TextureImporterTest, ExplicitColorSpacePersistsInKtxTransferFunction)
{
    const nlohmann::json srgbMeta = {
        {"importOption", {{"colorSpace", "srgb"}, {"generateMipmap", false}}}
    };
    const nlohmann::json linearMeta = {
        {"importOption", {{"colorSpace", "linear"}, {"generateMipmap", false}}}
    };

    EXPECT_EQ(ImportPpm(srgbMeta, "texture.ppm").transfer, KHR_DF_TRANSFER_SRGB);
    EXPECT_EQ(ImportPpm(linearMeta, "texture.ppm").transfer, KHR_DF_TRANSFER_LINEAR);
}

TEST(TextureImporterTest, LegacyLinearFormatMigratesDeterministically)
{
    const nlohmann::json legacySRGB = {
        {"importOption", {{"linearFormat", false}, {"generateMipmap", false}}}
    };
    const nlohmann::json legacyLinear = {
        {"importOption", {{"linearFormat", true}, {"generateMipmap", false}}}
    };

    const ImportedPpm srgb = ImportPpm(legacySRGB, "texture.ppm");
    const ImportedPpm linear = ImportPpm(legacyLinear, "texture.ppm");
    EXPECT_EQ(srgb.transfer, KHR_DF_TRANSFER_SRGB);
    EXPECT_EQ(linear.transfer, KHR_DF_TRANSFER_LINEAR);
    EXPECT_EQ(srgb.meta["importOption"]["colorSpace"], "srgb");
    EXPECT_EQ(linear.meta["importOption"]["colorSpace"], "linear");
    EXPECT_FALSE(srgb.meta["importOption"].contains("linearFormat"));
    EXPECT_FALSE(linear.meta["importOption"].contains("linearFormat"));
}

TEST(TextureImporterTest, MissingColorSpaceIsInferredFromSemanticName)
{
    const nlohmann::json meta = {
        {"importOption", {{"generateMipmap", false}}}
    };

    const ImportedPpm baseColor = ImportPpm(meta, "basecolor.ppm");
    const ImportedPpm normal = ImportPpm(meta, "normal.ppm");
    EXPECT_EQ(baseColor.transfer, KHR_DF_TRANSFER_SRGB);
    EXPECT_EQ(normal.transfer, KHR_DF_TRANSFER_LINEAR);
    EXPECT_EQ(baseColor.meta["importOption"]["colorSpace"], "srgb");
    EXPECT_EQ(normal.meta["importOption"]["colorSpace"], "linear");
}
