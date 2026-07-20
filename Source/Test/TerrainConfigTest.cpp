#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include "Editor/Tools/TerrainLayerPainting.hpp"
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

TEST(TerrainConfigTest, HeightRevisionTracksCollisionGeometryChanges)
{
    BinaryAsset binary;
    TerrainConfig config;

    uint64_t revision = config.GetHeightRevision();
    config.SetHeightDataAsset(&binary);
    EXPECT_GT(config.GetHeightRevision(), revision);

    revision = config.GetHeightRevision();
    ASSERT_TRUE(config.InitializeFlatHeightMap());
    EXPECT_GT(config.GetHeightRevision(), revision);

    revision = config.GetHeightRevision();
    const uint16_t originalHeight = config.GetHeightSamples().front();
    const TerrainHeightValue noOpEdit[] = {{0, originalHeight}};
    config.ApplyHeightValues(noOpEdit);
    EXPECT_EQ(config.GetHeightRevision(), revision);

    const TerrainHeightValue heightEdit[] = {{0, static_cast<uint16_t>(originalHeight + 1)}};
    config.ApplyHeightValues(heightEdit);
    EXPECT_GT(config.GetHeightRevision(), revision);

    revision = config.GetHeightRevision();
    config.SetSize(float2(200.0f, 150.0f));
    EXPECT_GT(config.GetHeightRevision(), revision);

    revision = config.GetHeightRevision();
    ASSERT_TRUE(config.SetHeightRange(float2(-64.0f, 64.0f), false));
    EXPECT_GT(config.GetHeightRevision(), revision);

    revision = config.GetHeightRevision();
    ASSERT_TRUE(config.SetVertexResolution(129));
    EXPECT_GT(config.GetHeightRevision(), revision);

    revision = config.GetHeightRevision();
    ASSERT_TRUE(config.ResizeHeightMap(128));
    EXPECT_GT(config.GetHeightRevision(), revision);

    revision = config.GetHeightRevision();
    config.SetSurface(float3(0.2f, 0.3f, 0.4f), 0.5f, 0.1f);
    config.AddLayer();
    EXPECT_EQ(config.GetHeightRevision(), revision);
}

TEST(TerrainConfigTest, RoundTripsVersionedLayerControlPayload)
{
    BinaryAsset binary;
    TerrainConfig source;
    source.SetLayerControlDataAsset(&binary);
    ASSERT_TRUE(source.ResizeLayerControlMaps(128));

    std::vector<TerrainLayerControlSample> ids(
        source.GetLayerIDSamples().begin(),
        source.GetLayerIDSamples().end()
    );
    std::vector<TerrainLayerControlSample> weights(
        source.GetLayerWeightSamples().begin(),
        source.GetLayerWeightSamples().end()
    );
    ids.front() = {4, 9, 255, 255};
    weights.front() = {192, 63, 0, 0};
    ids.back() = {17, 255, 255, 255};
    weights.back() = {255, 0, 0, 0};
    ASSERT_TRUE(source.SetLayerControlSamples(ids, weights));
    ASSERT_TRUE(source.CommitLayerControlData());
    EXPECT_EQ(
        binary.GetSize(),
        20 + source.GetLayerIDSamples().size() * sizeof(TerrainLayerControlSample) * 2
    );

    TerrainConfig loaded;
    loaded.ResizeLayerControlMaps(128);
    loaded.SetLayerControlDataAsset(&binary);
    ASSERT_TRUE(loaded.HasValidLayerControlData());
    EXPECT_EQ(loaded.GetLayerIDSamples().front(), ids.front());
    EXPECT_EQ(loaded.GetLayerWeightSamples().front(), weights.front());
    EXPECT_EQ(loaded.GetLayerIDSamples().back(), ids.back());
    EXPECT_EQ(loaded.GetLayerWeightSamples().back(), weights.back());
}

TEST(TerrainConfigTest, LayerControlResizePreservesPairedCorners)
{
    BinaryAsset binary;
    TerrainConfig config;
    config.SetLayerControlDataAsset(&binary);
    ASSERT_TRUE(config.InitializeLayerControlMaps());

    std::vector<TerrainLayerControlSample> ids(config.GetLayerIDSamples().begin(), config.GetLayerIDSamples().end());
    std::vector<TerrainLayerControlSample> weights(
        config.GetLayerWeightSamples().begin(),
        config.GetLayerWeightSamples().end()
    );
    ids.front() = {1, 2, 3, 4};
    weights.front() = {10, 20, 30, 40};
    ids.back() = {5, 6, 7, 8};
    weights.back() = {50, 60, 70, 80};
    ASSERT_TRUE(config.SetLayerControlSamples(ids, weights));
    ASSERT_TRUE(config.ResizeLayerControlMaps(128));

    EXPECT_EQ(config.GetLayerIDSamples().front(), ids.front());
    EXPECT_EQ(config.GetLayerWeightSamples().front(), weights.front());
    EXPECT_EQ(config.GetLayerIDSamples().back(), ids.back());
    EXPECT_EQ(config.GetLayerWeightSamples().back(), weights.back());
}

TEST(TerrainConfigTest, TerrainLayerIDsRemainStable)
{
    TerrainConfig config;
    EXPECT_EQ(config.AddLayer(), 0);
    EXPECT_EQ(config.AddLayer(), 1);
    EXPECT_EQ(config.AddLayer(), 2);
    ASSERT_TRUE(config.RemoveLayer(1));
    ASSERT_EQ(config.GetLayers().size(), 2);
    EXPECT_EQ(config.GetLayers()[0].id, 0);
    EXPECT_EQ(config.GetLayers()[1].id, 2);
    EXPECT_EQ(config.AddLayer(), 1);
    EXPECT_EQ(config.GetLayers().back().id, 1);

    while (config.GetLayers().size() < TerrainConfig::MaxTerrainLayers)
        ASSERT_NE(config.AddLayer(), TerrainConfig::InvalidTerrainLayerID);
    EXPECT_EQ(config.AddLayer(), TerrainConfig::InvalidTerrainLayerID);
}

TEST(TerrainConfigTest, RejectsInvalidLayerControlPayloadHeader)
{
    BinaryAsset binary;
    binary.SetData(std::vector<uint8_t>(20, 0));

    TerrainConfig config;
    config.SetLayerControlDataAsset(&binary);
    EXPECT_FALSE(config.HasValidLayerControlData());
}

TEST(TerrainConfigTest, AppliesPairedLayerControlValuesInBatches)
{
    BinaryAsset binary;
    TerrainConfig config;
    config.SetLayerControlDataAsset(&binary);
    ASSERT_TRUE(config.ResizeLayerControlMaps(128));
    const std::vector<uint8_t> committedBefore = binary.GetData();

    const TerrainLayerControlSample untouchedIDs = config.GetLayerIDSamples()[1];
    const TerrainLayerControlSample untouchedWeights = config.GetLayerWeightSamples()[1];
    const TerrainLayerControlValue changes[] = {
        {0, {1, 2, 255, 255}, {200, 55, 0, 0}},
        {127 * 128 + 127, {9, 8, 7, 6}, {100, 80, 50, 25}},
    };
    config.ApplyLayerControlValues(changes);

    EXPECT_EQ(config.GetLayerIDSamples()[0], changes[0].ids);
    EXPECT_EQ(config.GetLayerWeightSamples()[0], changes[0].weights);
    EXPECT_EQ(config.GetLayerIDSamples().back(), changes[1].ids);
    EXPECT_EQ(config.GetLayerWeightSamples().back(), changes[1].weights);
    EXPECT_EQ(config.GetLayerIDSamples()[1], untouchedIDs);
    EXPECT_EQ(config.GetLayerWeightSamples()[1], untouchedWeights);
    EXPECT_EQ(binary.GetData(), committedBefore);

    ASSERT_TRUE(config.CommitLayerControlData());
    EXPECT_NE(binary.GetData(), committedBefore);

    TerrainConfig loaded;
    loaded.ResizeLayerControlMaps(128);
    loaded.SetLayerControlDataAsset(&binary);
    ASSERT_TRUE(loaded.HasValidLayerControlData());
    EXPECT_EQ(loaded.GetLayerIDSamples()[0], changes[0].ids);
    EXPECT_EQ(loaded.GetLayerWeightSamples().back(), changes[1].weights);
}

TEST(TerrainConfigTest, LayerPaintingBlendsSelectedLayerAndPreservesWeightTotal)
{
    auto working = Editor::TerrainLayerPainting::CreateWorkingSample(
        {1, 2, 255, 255},
        {192, 63, 0, 0}
    );
    const size_t selectedChannel = Editor::TerrainLayerPainting::PrepareSelectedLayer(working, 2);
    Editor::TerrainLayerPainting::IntegrateSelectedLayer(working, selectedChannel, 6.0f, 1.0f / 60.0f, 0.75f);
    const auto painted = Editor::TerrainLayerPainting::Quantize(working);
    const auto weights = Editor::TerrainLayerPainting::ToArray(painted.weights);

    EXPECT_GT(weights[selectedChannel], 63);
    EXPECT_LT(weights[0], 192);
    EXPECT_EQ(static_cast<uint32_t>(weights[0]) + weights[1] + weights[2] + weights[3], 255);
}

TEST(TerrainConfigTest, LayerPaintingUsesInvalidSlotBeforeReplacingSmallestLayer)
{
    auto withInvalidSlot = Editor::TerrainLayerPainting::CreateWorkingSample(
        {1, 255, 3, 4},
        {100, 0, 80, 75}
    );
    const size_t invalidSlot = Editor::TerrainLayerPainting::PrepareSelectedLayer(withInvalidSlot, 9);
    EXPECT_EQ(invalidSlot, 1);
    EXPECT_EQ(withInvalidSlot.ids[invalidSlot], 9);

    auto full = Editor::TerrainLayerPainting::CreateWorkingSample(
        {1, 2, 3, 4},
        {100, 80, 60, 15}
    );
    const size_t replacedSlot = Editor::TerrainLayerPainting::PrepareSelectedLayer(full, 9);
    EXPECT_EQ(replacedSlot, 3);
    EXPECT_EQ(full.ids[replacedSlot], 9);
}

TEST(TerrainConfigTest, LayerPaintingLeavesFullySelectedLayerUnchanged)
{
    auto working = Editor::TerrainLayerPainting::CreateWorkingSample(
        {7, 255, 255, 255},
        {255, 0, 0, 0}
    );
    const size_t selectedChannel = Editor::TerrainLayerPainting::PrepareSelectedLayer(working, 7);
    Editor::TerrainLayerPainting::IntegrateSelectedLayer(working, selectedChannel, 20.0f, 0.05f, 1.0f);
    const auto painted = Editor::TerrainLayerPainting::Quantize(working, true);

    EXPECT_EQ(painted.ids, (TerrainLayerControlSample{7, 255, 255, 255}));
    EXPECT_EQ(painted.weights, (TerrainLayerControlSample{255, 0, 0, 0}));
}

TEST(TerrainConfigTest, LayerPaintingAccumulatesFractionalR8Progress)
{
    auto working = Editor::TerrainLayerPainting::CreateWorkingSample(
        {1, 255, 255, 255},
        {255, 0, 0, 0}
    );
    const size_t selectedChannel = Editor::TerrainLayerPainting::PrepareSelectedLayer(working, 2);
    TerrainLayerControlSample previousWeights{255, 0, 0, 0};
    bool producedR8Change = false;
    for (int iteration = 0; iteration < 1000; ++iteration)
    {
        Editor::TerrainLayerPainting::IntegrateSelectedLayer(
            working,
            selectedChannel,
            0.01f,
            0.001f,
            1.0f
        );
        const auto painted = Editor::TerrainLayerPainting::Quantize(working);
        if (painted.weights != previousWeights)
        {
            producedR8Change = true;
            break;
        }
    }
    EXPECT_TRUE(producedR8Change);
}
