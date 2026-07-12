#include "Engine/Runtime/System/AssetDatabase/Importers/ModelSubAssetUUIDAllocator.hpp"
#include <gtest/gtest.h>

TEST(ModelSubAssetUUIDAllocatorTest, ReusesExistingUniqueMapping)
{
    UUID existing;
    std::unordered_map<std::string, UUID> mappings{{"river-Material", existing}};
    ModelSubAssetUUIDAllocator allocator;
    allocator.Reset(&mappings);

    EXPECT_EQ(allocator.GetOrCreate("river-Material"), existing);
}

TEST(ModelSubAssetUUIDAllocatorTest, CreatesAndPersistsUUIDForMissingMapping)
{
    std::unordered_map<std::string, UUID> mappings;
    ModelSubAssetUUIDAllocator allocator;
    allocator.Reset(&mappings);

    UUID generated = allocator.GetOrCreate("island-Material");

    EXPECT_FALSE(generated.IsEmpty());
    ASSERT_TRUE(mappings.contains("island-Material"));
    EXPECT_EQ(mappings.at("island-Material"), generated);
}

TEST(ModelSubAssetUUIDAllocatorTest, RepairsDuplicateExistingMapping)
{
    UUID duplicate;
    std::unordered_map<std::string, UUID> mappings{
        {"first-Material", duplicate},
        {"second-Material", duplicate},
    };
    ModelSubAssetUUIDAllocator allocator;
    allocator.Reset(&mappings);

    EXPECT_EQ(allocator.GetOrCreate("first-Material"), duplicate);
    UUID repaired = allocator.GetOrCreate("second-Material");

    EXPECT_FALSE(repaired.IsEmpty());
    EXPECT_NE(repaired, duplicate);
    EXPECT_EQ(mappings.at("second-Material"), repaired);
}

TEST(ModelSubAssetUUIDAllocatorTest, GeneratedUUIDDoesNotReuseStaleMapping)
{
    UUID stale;
    std::unordered_map<std::string, UUID> mappings{{"removed-Material", stale}};
    ModelSubAssetUUIDAllocator allocator;
    allocator.Reset(&mappings);

    EXPECT_NE(allocator.GetOrCreate("new-Material"), stale);
}

TEST(ModelSubAssetUUIDAllocatorTest, SameNameWithDifferentTypesUsesIndependentMappings)
{
    std::unordered_map<std::string, UUID> mappings;
    ModelSubAssetUUIDAllocator allocator;
    allocator.Reset(&mappings);

    UUID material = allocator.GetOrCreate("shared-Material");
    UUID mesh = allocator.GetOrCreate("shared-Mesh");

    EXPECT_NE(material, mesh);
    EXPECT_EQ(mappings.at("shared-Material"), material);
    EXPECT_EQ(mappings.at("shared-Mesh"), mesh);
}

TEST(ModelSubAssetUUIDAllocatorTest, ReusesGeneratedMappingInSubsequentImport)
{
    std::unordered_map<std::string, UUID> mappings;
    ModelSubAssetUUIDAllocator firstImport;
    firstImport.Reset(&mappings);
    UUID generated = firstImport.GetOrCreate("island-Material");

    ModelSubAssetUUIDAllocator secondImport;
    secondImport.Reset(&mappings);

    EXPECT_EQ(secondImport.GetOrCreate("island-Material"), generated);
}
