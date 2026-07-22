#include "Engine/Library/ColorSpace.hpp"
#include "Engine/Library/Image/ImageProcessing.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include <filesystem>
#include <gtest/gtest.h>
#include <ktx.h>
#include <vulkan/vulkan.h>
#include <ktxvulkan.h>

TEST(ColorSpaceTest, ExactReferenceValuesAndThresholds)
{
    EXPECT_NEAR(ColorSpace::SRGBToLinear(0.04045f), 0.003130805f, 1.0e-7f);
    EXPECT_NEAR(ColorSpace::SRGBToLinear(0.5f), 0.21404114f, 1.0e-7f);
    EXPECT_NEAR(ColorSpace::LinearToSRGB(0.0031308f), 0.04044994f, 1.0e-7f);
    EXPECT_NEAR(ColorSpace::LinearToSRGB(0.18f), 0.46135613f, 1.0e-7f);
}

TEST(ColorSpaceTest, RGBARoundTripKeepsAlphaLinear)
{
    const glm::vec4 srgb{0.0f, 0.18f, 0.75f, 0.37f};
    const glm::vec4 roundTrip = ColorSpace::LinearToSRGB(ColorSpace::SRGBToLinear(srgb));
    EXPECT_NEAR(roundTrip.r, srgb.r, 1.0e-6f);
    EXPECT_NEAR(roundTrip.g, srgb.g, 1.0e-6f);
    EXPECT_NEAR(roundTrip.b, srgb.b, 1.0e-6f);
    EXPECT_FLOAT_EQ(roundTrip.a, srgb.a);
}

TEST(ColorSpaceTest, SRGBMipFilteringUsesLinearLightAndFiltersAlphaDirectly)
{
    uint8_t source[] = {
        0, 0, 0, 0,
        255, 255, 255, 255,
        0, 0, 0, 0,
        255, 255, 255, 255,
    };

    uint8_t* srgbMips = nullptr;
    size_t srgbByteSize = 0;
    Libs::Image::GenerateBoxFilteredMipmapSRGB8(source, 2, 2, 1, 2, 4, srgbMips, srgbByteSize);
    ASSERT_EQ(srgbByteSize, 20);
    EXPECT_NEAR(srgbMips[16], 188, 1);
    EXPECT_NEAR(srgbMips[17], 188, 1);
    EXPECT_NEAR(srgbMips[18], 188, 1);
    EXPECT_NEAR(srgbMips[19], 128, 1);
    delete[] srgbMips;

    uint8_t* linearMips = nullptr;
    size_t linearByteSize = 0;
    Libs::Image::GenerateBoxFilteredMipmap<uint8_t>(source, 2, 2, 1, 2, 4, linearMips, linearByteSize);
    ASSERT_EQ(linearByteSize, 20);
    EXPECT_NEAR(linearMips[16], 128, 1);
    delete[] linearMips;
}

TEST(ColorSpaceTest, DisplayTransformDefaultsAndModeValuesRemainCompatible)
{
    EXPECT_EQ(static_cast<uint32_t>(Rendering::RenderPipelineSetting::TonemapMode::ACES), 0u);
    EXPECT_EQ(static_cast<uint32_t>(Rendering::RenderPipelineSetting::TonemapMode::TonyMcMapface), 1u);
    EXPECT_EQ(static_cast<uint32_t>(Rendering::RenderPipelineSetting::TonemapMode::None), 2u);

    Rendering::RenderPipelineSetting::PostProcess settings;
    EXPECT_EQ(settings.tonemapMode, 1u);
    EXPECT_FLOAT_EQ(settings.exposureEV, 0.0f);
}

TEST(ColorSpaceTest, DisplayTransformSettingsRoundTripAndLegacyExposureDefaults)
{
    Rendering::RenderPipelineSetting::PostProcess original;
    original.tonemapMode = static_cast<uint32_t>(Rendering::RenderPipelineSetting::TonemapMode::None);
    original.exposureEV = 1.25f;

    JsonSerializer writer;
    original.Serialize(&writer);
    EXPECT_EQ(writer.GetJson()["tonemapMode"], 2u);
    EXPECT_FLOAT_EQ(writer.GetJson()["exposureEV"], 1.25f);

    Rendering::RenderPipelineSetting::PostProcess restored;
    JsonSerializer reader(writer.GetJson());
    restored.Deserialize(&reader);
    EXPECT_EQ(restored.tonemapMode, 2u);
    EXPECT_FLOAT_EQ(restored.exposureEV, 1.25f);

    Rendering::RenderPipelineSetting::PostProcess legacy;
    JsonSerializer legacyReader(nlohmann::json{{"tonemapMode", 0u}});
    legacy.Deserialize(&legacyReader);
    EXPECT_EQ(legacy.tonemapMode, 0u);
    EXPECT_FLOAT_EQ(legacy.exposureEV, 0.0f);
}

TEST(ColorSpaceTest, TonyLutIsLinearUnormAndSamplesAsLinearData)
{
    const std::filesystem::path engineRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path lutPath = engineRoot / "Assets/Textures/tony_mc_mapface.ktx";
    ktxTexture2* texture = nullptr;
    ASSERT_EQ(
        ktxTexture2_CreateFromNamedFile(
            lutPath.string().c_str(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &texture
        ),
        KTX_SUCCESS
    );

    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(ktxTexture2_GetVkFormat(texture), VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(ktxTexture2_GetOETF_e(texture), KHR_DF_TRANSFER_LINEAR);
    EXPECT_EQ(texture->baseWidth, 48u);
    EXPECT_EQ(texture->baseHeight, 48u);
    EXPECT_EQ(texture->baseDepth, 48u);
    EXPECT_EQ(texture->numLevels, 1u);
    EXPECT_EQ(ktxTexture_GetDataSize(ktxTexture(texture)), 442368u);

    const float stimulus = 0.306f;
    const float coordinate = stimulus / (stimulus + 1.0f) * 47.0f;
    const uint32_t low = static_cast<uint32_t>(coordinate);
    const uint32_t high = low + 1;
    const float fraction = coordinate - static_cast<float>(low);
    const uint8_t* data = ktxTexture_GetData(ktxTexture(texture));
    float sampled = 0.0f;
    for (uint32_t z : {low, high})
    {
        for (uint32_t y : {low, high})
        {
            for (uint32_t x : {low, high})
            {
                const float wx = x == low ? 1.0f - fraction : fraction;
                const float wy = y == low ? 1.0f - fraction : fraction;
                const float wz = z == low ? 1.0f - fraction : fraction;
                const size_t offset = ((static_cast<size_t>(z) * 48 * 48 + y * 48 + x) * 4);
                sampled += wx * wy * wz * static_cast<float>(data[offset]) / 255.0f;
            }
        }
    }
    EXPECT_NEAR(sampled, 0.263f, 0.002f);
    EXPECT_GT(sampled, ColorSpace::SRGBToLinear(sampled) * 4.0f);

    ktxTexture_Destroy(ktxTexture(texture));
}
