#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"

#include <gtest/gtest.h>

namespace
{
NavCell MakeCell(float height, const float4& edgeSlop, bool valid)
{
    NavCell cell;
    cell.height = height;
    cell.edgeSlop = edgeSlop;
    cell.valid = valid;
    return cell;
}

void ExpectCellEquals(const NavCell& actual, const NavCell& expected)
{
    EXPECT_FLOAT_EQ(actual.height, expected.height);
    EXPECT_EQ(actual.edgeSlop, expected.edgeSlop);
    EXPECT_EQ(actual.valid, expected.valid);
}
} // namespace

TEST(NavDataBinaryTest, RoundTripsVersionedCellPayload)
{
    BinaryAsset binaryAsset;
    NavData source;
    source.SetCellAsset(&binaryAsset);
    source.grid.config.width = 2;
    source.grid.config.height = 1;
    source.grid.cells = {
        MakeCell(1.25f, float4(0.1f, 0.2f, 0.3f, 0.4f), true),
        MakeCell(-3.5f, float4(-0.1f, -0.2f, -0.3f, -0.4f), false),
    };

    ASSERT_TRUE(source.WriteCellsToBinary());
    EXPECT_EQ(binaryAsset.GetSize(), 16 + 2 * 24);

    NavData loaded;
    loaded.SetCellAsset(&binaryAsset);
    loaded.grid.config = source.grid.config;
    loaded.OnLoaded();

    ASSERT_EQ(loaded.grid.cells.size(), source.grid.cells.size());
    ExpectCellEquals(loaded.grid.cells[0], source.grid.cells[0]);
    ExpectCellEquals(loaded.grid.cells[1], source.grid.cells[1]);
}

TEST(NavDataBinaryTest, JsonContainsReferenceButNotCells)
{
    BinaryAsset binaryAsset;
    NavData navData;
    navData.SetCellAsset(&binaryAsset);
    navData.grid.cells.push_back(MakeCell(2.0f, float4(1.0f), true));

    JsonSerializer serializer;
    static_cast<const Asset&>(navData).Serialize(&serializer);
    const nlohmann::json& json = serializer.GetJson();

    ASSERT_TRUE(json.contains("grid"));
    EXPECT_FALSE(json["grid"].contains("cells"));
    EXPECT_EQ(json["cellAsset"], binaryAsset.GetUUID().ToString());
}

TEST(NavDataBinaryTest, RejectsUnsupportedVersion)
{
    BinaryAsset binaryAsset;
    NavData source;
    source.SetCellAsset(&binaryAsset);
    ASSERT_TRUE(source.WriteCellsToBinary());

    std::vector<uint8_t> corrupted = binaryAsset.GetData();
    corrupted[4] = 2;
    binaryAsset.SetData(std::move(corrupted));

    NavData loaded;
    loaded.SetCellAsset(&binaryAsset);
    loaded.grid.cells.push_back({});
    loaded.OnLoaded();
    EXPECT_TRUE(loaded.grid.cells.empty());
}

TEST(NavDataBinaryTest, RejectsInvalidMagic)
{
    BinaryAsset binaryAsset;
    NavData source;
    source.SetCellAsset(&binaryAsset);
    ASSERT_TRUE(source.WriteCellsToBinary());

    std::vector<uint8_t> corrupted = binaryAsset.GetData();
    corrupted[0] = 'X';
    binaryAsset.SetData(std::move(corrupted));

    NavData loaded;
    loaded.SetCellAsset(&binaryAsset);
    loaded.grid.cells.push_back({});
    loaded.OnLoaded();
    EXPECT_TRUE(loaded.grid.cells.empty());
}

TEST(NavDataBinaryTest, RejectsInvalidRecordSize)
{
    BinaryAsset binaryAsset;
    NavData source;
    source.SetCellAsset(&binaryAsset);
    ASSERT_TRUE(source.WriteCellsToBinary());

    std::vector<uint8_t> corrupted = binaryAsset.GetData();
    corrupted[8] = 23;
    binaryAsset.SetData(std::move(corrupted));

    NavData loaded;
    loaded.SetCellAsset(&binaryAsset);
    loaded.grid.cells.push_back({});
    loaded.OnLoaded();
    EXPECT_TRUE(loaded.grid.cells.empty());
}

TEST(NavDataBinaryTest, RejectsTruncatedPayload)
{
    BinaryAsset binaryAsset;
    NavData source;
    source.SetCellAsset(&binaryAsset);
    source.grid.config.width = 1;
    source.grid.config.height = 1;
    source.grid.cells.push_back(MakeCell(1.0f, float4(0.0f), true));
    ASSERT_TRUE(source.WriteCellsToBinary());

    std::vector<uint8_t> corrupted = binaryAsset.GetData();
    corrupted.pop_back();
    binaryAsset.SetData(std::move(corrupted));

    NavData loaded;
    loaded.SetCellAsset(&binaryAsset);
    loaded.grid.config = source.grid.config;
    loaded.OnLoaded();
    EXPECT_TRUE(loaded.grid.cells.empty());
}

TEST(NavDataBinaryTest, RejectsCellCountThatDoesNotMatchGrid)
{
    BinaryAsset binaryAsset;
    NavData source;
    source.SetCellAsset(&binaryAsset);
    source.grid.config.width = 1;
    source.grid.config.height = 1;
    source.grid.cells.push_back(MakeCell(1.0f, float4(0.0f), true));
    ASSERT_TRUE(source.WriteCellsToBinary());

    NavData loaded;
    loaded.SetCellAsset(&binaryAsset);
    loaded.grid.config.width = 2;
    loaded.grid.config.height = 1;
    loaded.OnLoaded();
    EXPECT_TRUE(loaded.grid.cells.empty());
}

TEST(NavDataBinaryTest, RejectsInvalidValidityValue)
{
    BinaryAsset binaryAsset;
    NavData source;
    source.SetCellAsset(&binaryAsset);
    source.grid.config.width = 1;
    source.grid.config.height = 1;
    source.grid.cells.push_back(MakeCell(1.0f, float4(0.0f), true));
    ASSERT_TRUE(source.WriteCellsToBinary());

    std::vector<uint8_t> corrupted = binaryAsset.GetData();
    corrupted[36] = 2;
    binaryAsset.SetData(std::move(corrupted));

    NavData loaded;
    loaded.SetCellAsset(&binaryAsset);
    loaded.grid.config = source.grid.config;
    loaded.OnLoaded();
    EXPECT_TRUE(loaded.grid.cells.empty());
}

TEST(NavDataBinaryTest, LegacyJsonCellsRequireRebake)
{
    nlohmann::json legacy = {
        {"grid",
         {
             {"config", {{"width", 1}, {"height", 1}}},
             {"cells", {{{"height", 4.0f}, {"edgeSlop", {0.0f, 0.0f, 0.0f, 0.0f}}, {"valid", true}}}},
         }},
    };
    JsonSerializer serializer(legacy);

    NavData loaded;
    static_cast<Asset&>(loaded).Deserialize(&serializer);
    loaded.OnLoaded();

    EXPECT_EQ(loaded.grid.config.width, 1);
    EXPECT_EQ(loaded.grid.config.height, 1);
    EXPECT_TRUE(loaded.grid.cells.empty());
    EXPECT_EQ(loaded.GetCellAsset(), nullptr);
}
