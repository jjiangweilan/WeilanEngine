#include "Engine/Runtime/System/AssetDatabase/ModelArtifact.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace
{
constexpr uint32_t MeshMagic = 0x4D534842;
constexpr uint32_t AnimationClipMagic = 0x41434C50;

struct BlobHeader
{
    uint32_t magic = 0;
    uint32_t version = 1;
    uint64_t jsonSize = 0;
};

PodVector<uint8_t> MakeBlob(uint32_t magic, const std::string& json, const std::vector<uint8_t>& payload = {})
{
    BlobHeader header{magic, 1, json.size()};
    PodVector<uint8_t> blob(sizeof(header) + json.size() + payload.size());
    std::memcpy(blob.data(), &header, sizeof(header));
    std::memcpy(blob.data() + sizeof(header), json.data(), json.size());
    if (!payload.empty())
    {
        std::memcpy(blob.data() + sizeof(header) + json.size(), payload.data(), payload.size());
    }
    return blob;
}

nlohmann::json MakeMeshHeader()
{
    return {
        {"name", "mesh"},
        {"vertexCount", 1},
        {"indexCount", 1},
        {"positionsOffset", 0},
        {"positionsSize", sizeof(glm::vec3)},
        {"attributesOffset", sizeof(glm::vec3)},
        {"attributesSize", 0},
        {"indicesOffset", sizeof(glm::vec3)},
        {"indicesSize", sizeof(uint32_t)},
        {"attributes", nlohmann::json::array()},
        {"aabb", {{"min", {0.0f, 0.0f, 0.0f}}, {"max", {1.0f, 1.0f, 1.0f}}}},
        {"skeleton", nlohmann::json::array()},
    };
}
} // namespace

TEST(ModelArtifactTest, RejectsInvalidJsonWithoutThrowing)
{
    auto blob = MakeBlob(MeshMagic, "{ invalid json");
    std::unique_ptr<Mesh> mesh;
    EXPECT_NO_THROW(mesh = ModelArtifact::ReadMeshBlob(blob));
    EXPECT_EQ(mesh, nullptr);
}

TEST(ModelArtifactTest, RejectsTruncatedJsonHeader)
{
    BlobHeader header{MeshMagic, 1, std::numeric_limits<uint64_t>::max()};
    PodVector<uint8_t> blob(sizeof(header));
    std::memcpy(blob.data(), &header, sizeof(header));

    EXPECT_EQ(ModelArtifact::ReadMeshBlob(blob), nullptr);
}

TEST(ModelArtifactTest, RejectsMismatchedMeshCountsAndSizes)
{
    auto header = MakeMeshHeader();
    header["positionsSize"] = sizeof(glm::vec3) - 1;
    std::vector<uint8_t> payload(sizeof(glm::vec3) + sizeof(uint32_t));
    auto blob = MakeBlob(MeshMagic, header.dump(), payload);

    EXPECT_EQ(ModelArtifact::ReadMeshBlob(blob), nullptr);
}

TEST(ModelArtifactTest, RejectsMismatchedIndexCountAndSize)
{
    auto header = MakeMeshHeader();
    header["indicesSize"] = sizeof(uint32_t) - 1;
    std::vector<uint8_t> payload(sizeof(glm::vec3) + sizeof(uint32_t));
    auto blob = MakeBlob(MeshMagic, header.dump(), payload);

    EXPECT_EQ(ModelArtifact::ReadMeshBlob(blob), nullptr);
}

TEST(ModelArtifactTest, RejectsAttributeStrideMismatch)
{
    auto header = MakeMeshHeader();
    header["attributes"] = nlohmann::json::array(
        {{{"name", "normal"}, {"semantic", 1}, {"semanticIndex", 0}, {"size", sizeof(glm::vec3)}}}
    );
    header["attributesSize"] = sizeof(glm::vec3) - 1;
    std::vector<uint8_t> payload(sizeof(glm::vec3) * 2 + sizeof(uint32_t));
    auto blob = MakeBlob(MeshMagic, header.dump(), payload);

    EXPECT_EQ(ModelArtifact::ReadMeshBlob(blob), nullptr);
}

TEST(ModelArtifactTest, RejectsOutOfRangeMeshOffset)
{
    auto header = MakeMeshHeader();
    header["indicesOffset"] = std::numeric_limits<uint64_t>::max();
    std::vector<uint8_t> payload(sizeof(glm::vec3) + sizeof(uint32_t));
    auto blob = MakeBlob(MeshMagic, header.dump(), payload);

    EXPECT_EQ(ModelArtifact::ReadMeshBlob(blob), nullptr);
}

TEST(ModelArtifactTest, RejectsTruncatedMeshPayload)
{
    auto header = MakeMeshHeader();
    std::vector<uint8_t> payload(sizeof(glm::vec3));
    auto blob = MakeBlob(MeshMagic, header.dump(), payload);

    EXPECT_EQ(ModelArtifact::ReadMeshBlob(blob), nullptr);
}

TEST(ModelArtifactTest, RejectsOverflowingAnimationKeyCount)
{
    nlohmann::json channel = {
        {"nodeName", "node"},
        {"positionsOffset", 0},
        {"positionsCount", std::numeric_limits<uint64_t>::max()},
        {"rotationsOffset", 0},
        {"rotationsCount", 0},
        {"scalingsOffset", 0},
        {"scalingsCount", 0},
    };
    nlohmann::json header = {
        {"name", "clip"},
        {"tickPerSecond", 1.0f},
        {"duration", 1.0f},
        {"channels", nlohmann::json::array({channel})},
    };
    auto blob = MakeBlob(AnimationClipMagic, header.dump());

    EXPECT_EQ(ModelArtifact::ReadAnimationClipBlob(blob), nullptr);
}

TEST(ModelArtifactTest, RejectsInvalidAnimationClipJson)
{
    auto blob = MakeBlob(AnimationClipMagic, "[]");
    EXPECT_EQ(ModelArtifact::ReadAnimationClipBlob(blob), nullptr);
}
