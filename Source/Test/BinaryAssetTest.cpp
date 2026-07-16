#include "Engine/Core/BinaryAsset.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace
{
class TemporaryBinaryFile
{
public:
    TemporaryBinaryFile()
        : path(std::filesystem::temp_directory_path() / (UUID().ToString() + ".bin"))
    {}

    ~TemporaryBinaryFile()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::filesystem::path path;
};
} // namespace

TEST(BinaryAssetTest, SavesAndLoadsExactBytes)
{
    TemporaryBinaryFile file;
    const std::vector<uint8_t> expected = {0, 1, 2, 127, 128, 255};

    BinaryAsset source;
    source.SetData(expected);
    ASSERT_TRUE(source.SaveToFile(file.path));

    BinaryAsset loaded;
    ASSERT_TRUE(loaded.LoadFromFile(file.path.string().c_str()));
    EXPECT_EQ(loaded.GetData(), expected);
}

TEST(BinaryAssetTest, EmptyAssetCreatesAndLoadsAZeroByteFile)
{
    TemporaryBinaryFile file;

    BinaryAsset source;
    ASSERT_TRUE(source.SaveToFile(file.path));
    ASSERT_TRUE(std::filesystem::exists(file.path));
    EXPECT_EQ(std::filesystem::file_size(file.path), 0);

    BinaryAsset loaded;
    ASSERT_TRUE(loaded.LoadFromFile(file.path.string().c_str()));
    EXPECT_TRUE(loaded.GetData().empty());
}

TEST(BinaryAssetTest, DeclaresBinExtension)
{
    ASSERT_EQ(BinaryAsset::StaticGetExtensions().size(), 1);
    EXPECT_EQ(BinaryAsset::StaticGetExtensions()[0], ".bin");
}
