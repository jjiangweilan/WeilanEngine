#include "Editor/Inspectors/MaterialAttributeParser.hpp"
#include <gtest/gtest.h>

TEST(MaterialAttributeParserTest, ParseColor)
{
    std::vector<std::string> attrs = { "Color" };
    auto info = Editor::ParseAttributes(attrs);
    EXPECT_TRUE(info.isColor);
}

TEST(MaterialAttributeParserTest, ParseRange)
{
    std::vector<std::string> attrs = { "Range(0.5, 2.0)" };
    auto info = Editor::ParseAttributes(attrs);
    ASSERT_TRUE(info.range.has_value());
    EXPECT_FLOAT_EQ(info.range->min, 0.5f);
    EXPECT_FLOAT_EQ(info.range->max, 2.0f);
}

TEST(MaterialAttributeParserTest, ParseRangeDefault)
{
    std::vector<std::string> attrs = { "Range" };
    auto info = Editor::ParseAttributes(attrs);
    ASSERT_TRUE(info.range.has_value());
    EXPECT_FLOAT_EQ(info.range->min, 0.0f);
    EXPECT_FLOAT_EQ(info.range->max, 1.0f);
}

TEST(MaterialAttributeParserTest, ParseGroup)
{
    std::vector<std::string> attrs = { "Group(\"Surface Settings\")" };
    auto info = Editor::ParseAttributes(attrs);
    ASSERT_TRUE(info.group.has_value());
    EXPECT_EQ(info.group->name, "Surface Settings");
}

TEST(MaterialAttributeParserTest, ParseTooltip)
{
    std::vector<std::string> attrs = { "Tooltip(\"The albedo color of the material\")" };
    auto info = Editor::ParseAttributes(attrs);
    ASSERT_TRUE(info.tooltip.has_value());
    EXPECT_EQ(info.tooltip->text, "The albedo color of the material");
}

TEST(MaterialAttributeParserTest, ParseMultiple)
{
    std::vector<std::string> attrs = { "Color", "Range(0, 1)", "Group(\"Params\")" };
    auto info = Editor::ParseAttributes(attrs);
    EXPECT_TRUE(info.isColor);
    ASSERT_TRUE(info.range.has_value());
    EXPECT_FLOAT_EQ(info.range->min, 0.0f);
    EXPECT_FLOAT_EQ(info.range->max, 1.0f);
    ASSERT_TRUE(info.group.has_value());
    EXPECT_EQ(info.group->name, "Params");
}

TEST(MaterialAttributeParserTest, CaseInsensitive)
{
    std::vector<std::string> attrs = { "color", "RANGE(0.1, 0.9)", "group(\"Test\")" };
    auto info = Editor::ParseAttributes(attrs);
    EXPECT_TRUE(info.isColor);
    ASSERT_TRUE(info.range.has_value());
    EXPECT_FLOAT_EQ(info.range->min, 0.1f);
    EXPECT_FLOAT_EQ(info.range->max, 0.9f);
    ASSERT_TRUE(info.group.has_value());
    EXPECT_EQ(info.group->name, "Test");
}
