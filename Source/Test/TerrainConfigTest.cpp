#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include <gtest/gtest.h>

namespace
{
float DecodeNormalComponent(uint8_t encoded)
{
    return static_cast<float>(encoded) / 255.0f * 2.0f - 1.0f;
}
} // namespace

TEST(TerrainConfigTest, RoundTripsVersionedR16HeightPayload)
{
    BinaryAsset binary;
    TerrainConfig source;
    source.SetHeightDataAsset(&binary);
    ASSERT_TRUE(source.InitializeFlatHeightMap());

    const uint32_t last = source.GetHeightMapResolution() * source.GetHeightMapResolution() - 1;
    const TerrainHeightValue edits[] = {{0, 0}, {last, 65535}};
    source.ApplyHeightValues(edits);
    ASSERT_TRUE(source.CommitHeightData());
    EXPECT_EQ(binary.GetSize(), 20 + source.GetHeightSamples().size() * sizeof(uint16_t));

    TerrainConfig loaded;
    loaded.SetHeightDataAsset(&binary);
    ASSERT_TRUE(loaded.HasValidHeightData());
    EXPECT_EQ(loaded.GetHeightSamples().front(), 0);
    EXPECT_EQ(loaded.GetHeightSamples().back(), 65535);
}

TEST(TerrainConfigTest, FlatMapRepresentsWorldHeightZeroInSignedRange)
{
    BinaryAsset binary;
    TerrainConfig config;
    config.SetHeightDataAsset(&binary);
    ASSERT_TRUE(config.SetHeightRange(float2(-10.0f, 30.0f)));
    ASSERT_TRUE(config.InitializeFlatHeightMap());

    EXPECT_NEAR(config.SampleHeight(float2(0.0f)), 0.0f, 0.001f);
    EXPECT_NEAR(config.SampleHeight(float2(0.37f, 0.81f)), 0.0f, 0.001f);

    const TerrainGeometryNormalSample normal = config.GetGeometryNormalSamples().front();
    EXPECT_NEAR(DecodeNormalComponent(normal.encodedX), 0.0f, 0.005f);
    EXPECT_NEAR(DecodeNormalComponent(normal.encodedZ), 0.0f, 0.005f);
}

TEST(TerrainConfigTest, GeometryNormalMapRespondsToHeightAndSizeChanges)
{
    BinaryAsset binary;
    TerrainConfig config;
    config.SetHeightDataAsset(&binary);
    ASSERT_TRUE(config.InitializeFlatHeightMap());

    const uint32_t resolution = config.GetHeightMapResolution();
    const uint32_t centerX = resolution / 2;
    const uint32_t centerY = resolution / 2;
    const TerrainHeightValue slope[] = {
        {centerY * resolution + centerX - 1, 32700},
        {centerY * resolution + centerX + 1, 32836},
    };
    config.ApplyHeightValues(slope);

    const TerrainGeometryNormalSample initialNormal =
        config.GetGeometryNormalSamples()[centerY * resolution + centerX];
    const float initialX = DecodeNormalComponent(initialNormal.encodedX);
    EXPECT_LT(initialX, 0.0f);
    EXPECT_NEAR(DecodeNormalComponent(initialNormal.encodedZ), 0.0f, 0.005f);

    config.SetSize(float2(200.0f, 100.0f));
    const TerrainGeometryNormalSample rescaledNormal =
        config.GetGeometryNormalSamples()[centerY * resolution + centerX];
    const float rescaledX = DecodeNormalComponent(rescaledNormal.encodedX);
    EXPECT_LT(rescaledX, 0.0f);
    EXPECT_LT(glm::abs(rescaledX), glm::abs(initialX));

    ASSERT_TRUE(config.SetHeightRange(float2(-64.0f, 64.0f), false));
    const TerrainGeometryNormalSample expandedRangeNormal =
        config.GetGeometryNormalSamples()[centerY * resolution + centerX];
    EXPECT_GT(glm::abs(DecodeNormalComponent(expandedRangeNormal.encodedX)), glm::abs(rescaledX));
}

TEST(TerrainConfigTest, ResizePreservesHeightMapCorners)
{
    BinaryAsset binary;
    TerrainConfig config;
    config.SetHeightDataAsset(&binary);
    ASSERT_TRUE(config.InitializeFlatHeightMap());

    const uint32_t oldResolution = config.GetHeightMapResolution();
    const TerrainHeightValue corners[] = {
        {0, 0},
        {oldResolution - 1, 16384},
        {(oldResolution - 1) * oldResolution, 49152},
        {oldResolution * oldResolution - 1, 65535},
    };
    config.ApplyHeightValues(corners);
    ASSERT_TRUE(config.ResizeHeightMap(128));

    const std::span<const uint16_t> samples = config.GetHeightSamples();
    EXPECT_EQ(config.GetGeometryNormalSamples().size(), samples.size());
    EXPECT_EQ(samples[0], 0);
    EXPECT_EQ(samples[127], 16384);
    EXPECT_EQ(samples[127 * 128], 49152);
    EXPECT_EQ(samples.back(), 65535);
}

TEST(TerrainConfigTest, RejectsInvalidHeightPayloadHeader)
{
    BinaryAsset binary;
    binary.SetData(std::vector<uint8_t>(20, 0));

    TerrainConfig config;
    config.SetHeightDataAsset(&binary);
    EXPECT_FALSE(config.HasValidHeightData());
}
