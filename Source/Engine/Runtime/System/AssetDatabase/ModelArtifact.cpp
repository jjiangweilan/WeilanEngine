#include "ModelArtifact.hpp"
#include "Engine/Driver/GfxDriver/VertexAttributes.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include <cstring>
#include <fmt/format.h>
#include <fstream>

namespace ModelArtifact
{
namespace
{
constexpr uint32_t MeshMagic = 0x4D534842;      // MSHB
constexpr uint32_t AnimationMagic = 0x414E494D; // ANIM
constexpr uint32_t BlobVersion = 1;

struct BlobHeader
{
    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t jsonSize = 0;
};

template <class T>
void AppendBytes(std::vector<uint8_t>& out, const T& value)
{
    const uint8_t* src = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), src, src + sizeof(T));
}

void AppendBytes(std::vector<uint8_t>& out, const void* data, size_t size)
{
    const uint8_t* src = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), src, src + size);
}

bool WriteBlob(const std::filesystem::path& path, uint32_t magic, const nlohmann::json& header, const std::vector<uint8_t>& payload)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.good())
    {
        return false;
    }

    std::string headerText = header.dump();
    BlobHeader blobHeader{magic, BlobVersion, headerText.size()};
    out.write(reinterpret_cast<const char*>(&blobHeader), sizeof(blobHeader));
    out.write(headerText.data(), static_cast<std::streamsize>(headerText.size()));
    if (!payload.empty())
    {
        out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }

    return out.good();
}

bool ReadBlob(const PodVector<uint8_t>& data, uint32_t expectedMagic, nlohmann::json& header, const uint8_t*& payload, size_t& payloadSize)
{
    if (data.size() < sizeof(BlobHeader))
    {
        return false;
    }

    const BlobHeader* blobHeader = reinterpret_cast<const BlobHeader*>(data.data());
    if (blobHeader->magic != expectedMagic || blobHeader->version != BlobVersion)
    {
        return false;
    }

    size_t headerOffset = sizeof(BlobHeader);
    size_t payloadOffset = headerOffset + static_cast<size_t>(blobHeader->jsonSize);
    if (payloadOffset > data.size())
    {
        return false;
    }

    header = nlohmann::json::parse(data.data() + headerOffset, data.data() + payloadOffset);
    payload = data.data() + payloadOffset;
    payloadSize = data.size() - payloadOffset;
    return true;
}

std::vector<float> Mat4ToVector(const glm::mat4& m)
{
    return {
        m[0][0], m[0][1], m[0][2], m[0][3],
        m[1][0], m[1][1], m[1][2], m[1][3],
        m[2][0], m[2][1], m[2][2], m[2][3],
        m[3][0], m[3][1], m[3][2], m[3][3],
    };
}

glm::mat4 VectorToMat4(const nlohmann::json& values)
{
    glm::mat4 m(1.0f);
    if (values.is_array() && values.size() >= 16)
    {
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                m[c][r] = values[c * 4 + r].get<float>();
            }
        }
    }
    return m;
}

struct PackedVec3Key
{
    double time;
    float x;
    float y;
    float z;
};

struct PackedQuatKey
{
    double time;
    float w;
    float x;
    float y;
    float z;
};

template <class KeyType>
const KeyType* ReadKeyRange(const uint8_t* payload, size_t payloadSize, size_t offset, size_t count)
{
    size_t size = count * sizeof(KeyType);
    if (offset + size > payloadSize)
    {
        return nullptr;
    }
    return reinterpret_cast<const KeyType*>(payload + offset);
}
} // namespace

bool WriteMeshBlob(const std::filesystem::path& path, const Mesh& mesh)
{
    const auto& submeshes = const_cast<Mesh&>(mesh).GetSubmeshes();
    if (submeshes.empty())
    {
        return false;
    }

    const Submesh& submesh = submeshes[0];
    const auto& positions = submesh.GetPositions();
    const auto& attributes = submesh.GetVertexAttribute();
    const auto& indices = submesh.GetIndices();
    const auto& attributeData = attributes.GetData();

    std::vector<uint8_t> payload;
    size_t positionsOffset = payload.size();
    AppendBytes(payload, positions.data(), positions.size() * sizeof(glm::vec3));
    size_t attributesOffset = payload.size();
    AppendBytes(payload, attributeData.data(), attributeData.size());
    size_t indicesOffset = payload.size();
    AppendBytes(payload, indices.data(), indices.size() * sizeof(uint32_t));

    nlohmann::json header;
    header["name"] = mesh.GetName();
    header["vertexCount"] = positions.size();
    header["indexCount"] = indices.size();
    header["positionsOffset"] = positionsOffset;
    header["positionsSize"] = positions.size() * sizeof(glm::vec3);
    header["attributesOffset"] = attributesOffset;
    header["attributesSize"] = attributeData.size();
    header["indicesOffset"] = indicesOffset;
    header["indicesSize"] = indices.size() * sizeof(uint32_t);
    header["aabb"]["min"] = {submesh.GetAABB().min.x, submesh.GetAABB().min.y, submesh.GetAABB().min.z};
    header["aabb"]["max"] = {submesh.GetAABB().max.x, submesh.GetAABB().max.y, submesh.GetAABB().max.z};

    for (const auto& attribute : attributes.GetDescription())
    {
        header["attributes"].push_back({
            {"name", attribute.name},
            {"semantic", static_cast<int>(attribute.semanticName)},
            {"semanticIndex", attribute.semanticIndex},
            {"size", attribute.size},
        });
    }

    const Skeleton& skeleton = const_cast<Mesh&>(mesh).GetSkeleton();
    for (const auto& bone : skeleton)
    {
        header["skeleton"].push_back({{"name", bone.name}, {"offsetMatrix", Mat4ToVector(bone.offsetMatrix)}});
    }

    return WriteBlob(path, MeshMagic, header, payload);
}

std::unique_ptr<Mesh> ReadMeshBlob(const PodVector<uint8_t>& data)
{
    nlohmann::json header;
    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
    if (!ReadBlob(data, MeshMagic, header, payload, payloadSize))
    {
        return nullptr;
    }

    auto readRange = [&](size_t offset, size_t size) -> const uint8_t*
    {
        if (offset + size > payloadSize)
        {
            return nullptr;
        }
        return payload + offset;
    };

    size_t vertexCount = header.value("vertexCount", 0);
    size_t indexCount = header.value("indexCount", 0);
    size_t positionsSize = header.value("positionsSize", 0);
    size_t attributesSize = header.value("attributesSize", 0);
    size_t indicesSize = header.value("indicesSize", 0);

    const uint8_t* positionBytes = readRange(header.value("positionsOffset", 0), positionsSize);
    const uint8_t* attributeBytes = readRange(header.value("attributesOffset", 0), attributesSize);
    const uint8_t* indexBytes = readRange(header.value("indicesOffset", 0), indicesSize);
    if (positionBytes == nullptr || attributeBytes == nullptr || indexBytes == nullptr)
    {
        return nullptr;
    }

    std::vector<glm::vec3> positions(vertexCount);
    std::memcpy(positions.data(), positionBytes, positionsSize);
    std::vector<uint8_t> attributeData(attributesSize);
    std::memcpy(attributeData.data(), attributeBytes, attributesSize);
    std::vector<uint32_t> indices(indexCount);
    std::memcpy(indices.data(), indexBytes, indicesSize);

    VertexAttributes attributes;
    for (const auto& attribute : header.value("attributes", nlohmann::json::array()))
    {
        attributes.AddAttribute(
            attribute.value("name", "").c_str(),
            static_cast<VertexAttributeSemantics>(attribute.value("semantic", 0)),
            attribute.value("semanticIndex", 0),
            attribute.value("size", 0)
        );
    }
    attributes.SetData(std::move(attributeData));

    Submesh submesh;
    submesh.SetPositions(std::move(positions));
    submesh.SetVertexAttribute(std::move(attributes));
    submesh.SetIndices(std::move(indices));
    AABB aabb;
    auto aabbMin = header["aabb"].value("min", nlohmann::json::array({0, 0, 0}));
    auto aabbMax = header["aabb"].value("max", nlohmann::json::array({0, 0, 0}));
    aabb.min = {aabbMin[0].get<float>(), aabbMin[1].get<float>(), aabbMin[2].get<float>()};
    aabb.max = {aabbMax[0].get<float>(), aabbMax[1].get<float>(), aabbMax[2].get<float>()};
    submesh.SetAABB(aabb);
    submesh.Apply();

    auto mesh = std::make_unique<Mesh>();
    mesh->SetName(header.value("name", "Mesh"));
    std::vector<Submesh> submeshes;
    submeshes.push_back(std::move(submesh));
    mesh->SetSubmeshes(std::move(submeshes));

    Skeleton skeleton;
    for (const auto& bone : header.value("skeleton", nlohmann::json::array()))
    {
        skeleton.push_back({bone.value("name", ""), VectorToMat4(bone["offsetMatrix"])});
    }
    mesh->SetSkeleton(std::move(skeleton));
    return mesh;
}

bool WriteAnimationBlob(const std::filesystem::path& path, const Animation& animation)
{
    nlohmann::json header;
    std::vector<uint8_t> payload;
    for (const auto& [clipName, clipPtr] : const_cast<Animation&>(animation).GetAnimationClips())
    {
        const auto& clip = *clipPtr;
        nlohmann::json clipJson;
        clipJson["name"] = clip.name;
        clipJson["tickPerSecond"] = clip.tickPerSecond;
        clipJson["duration"] = clip.duration;

        for (const auto& channel : clip.channels)
        {
            nlohmann::json channelJson;
            channelJson["nodeName"] = channel.nodeName;

            channelJson["positionsOffset"] = payload.size();
            channelJson["positionsCount"] = channel.positions.size();
            for (const auto& key : channel.positions)
            {
                AppendBytes(payload, PackedVec3Key{key.time, key.val.x, key.val.y, key.val.z});
            }

            channelJson["rotationsOffset"] = payload.size();
            channelJson["rotationsCount"] = channel.rotations.size();
            for (const auto& key : channel.rotations)
            {
                AppendBytes(payload, PackedQuatKey{key.time, key.val.w, key.val.x, key.val.y, key.val.z});
            }

            channelJson["scalingsOffset"] = payload.size();
            channelJson["scalingsCount"] = channel.scalings.size();
            for (const auto& key : channel.scalings)
            {
                AppendBytes(payload, PackedVec3Key{key.time, key.val.x, key.val.y, key.val.z});
            }

            clipJson["channels"].push_back(std::move(channelJson));
        }

        header["clips"].push_back(std::move(clipJson));
    }

    return WriteBlob(path, AnimationMagic, header, payload);
}

std::unique_ptr<Animation> ReadAnimationBlob(const PodVector<uint8_t>& data)
{
    nlohmann::json header;
    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
    if (!ReadBlob(data, AnimationMagic, header, payload, payloadSize))
    {
        return nullptr;
    }

    auto animation = std::make_unique<Animation>();
    for (const auto& clipJson : header.value("clips", nlohmann::json::array()))
    {
        std::vector<Animation::Channel> channels;
        for (const auto& channelJson : clipJson.value("channels", nlohmann::json::array()))
        {
            Animation::Channel channel;
            channel.nodeName = channelJson.value("nodeName", "");

            size_t positionsCount = channelJson.value("positionsCount", 0);
            auto positions = ReadKeyRange<PackedVec3Key>(payload, payloadSize, channelJson.value("positionsOffset", 0), positionsCount);
            if (positions != nullptr)
            {
                for (size_t i = 0; i < positionsCount; ++i)
                {
                    channel.positions.push_back({positions[i].time, {positions[i].x, positions[i].y, positions[i].z}});
                }
            }

            size_t rotationsCount = channelJson.value("rotationsCount", 0);
            auto rotations = ReadKeyRange<PackedQuatKey>(payload, payloadSize, channelJson.value("rotationsOffset", 0), rotationsCount);
            if (rotations != nullptr)
            {
                for (size_t i = 0; i < rotationsCount; ++i)
                {
                    channel.rotations.push_back({rotations[i].time, {rotations[i].w, rotations[i].x, rotations[i].y, rotations[i].z}});
                }
            }

            size_t scalingsCount = channelJson.value("scalingsCount", 0);
            auto scalings = ReadKeyRange<PackedVec3Key>(payload, payloadSize, channelJson.value("scalingsOffset", 0), scalingsCount);
            if (scalings != nullptr)
            {
                for (size_t i = 0; i < scalingsCount; ++i)
                {
                    channel.scalings.push_back({scalings[i].time, {scalings[i].x, scalings[i].y, scalings[i].z}});
                }
            }

            channels.push_back(std::move(channel));
        }

        animation->AddClip(
            clipJson.value("name", "animation"),
            clipJson.value("tickPerSecond", 0.0f),
            clipJson.value("duration", 0.0f),
            channels
        );
    }

    return animation;
}

bool WriteModelGraph(
    const std::filesystem::path& path,
    const std::vector<std::unique_ptr<GameObject>>& gameObjects,
    const std::vector<ObjPtr<GameObject>>& roots
)
{
    JsonSerializer ser;
    Serializer& serializer = ser;
    serializer.Serialize("gameObjects", gameObjects);
    serializer.Serialize("roots", roots);
    auto binary = ser.GetBinary();

    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.good())
    {
        return false;
    }
    out.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
    return out.good();
}

bool ReadModelGraph(
    const PodVector<uint8_t>& data,
    std::vector<std::unique_ptr<GameObject>>& gameObjects,
    std::vector<ObjPtr<GameObject>>& roots
)
{
    if (data.size() == 0)
    {
        return false;
    }

    SerializeReferenceResolveMap resolveMap;
    std::vector<uint8_t> binary(data.data(), data.data() + data.size());
    JsonSerializer ser(binary, &resolveMap);
    Serializer& serializer = ser;
    serializer.Deserialize("gameObjects", gameObjects);
    serializer.Deserialize("roots", roots);

    for (auto& [uuid, resolves] : resolveMap)
    {
        auto objectIter = ser.GetContainedObjects().find(uuid);
        if (objectIter == ser.GetContainedObjects().end())
        {
            continue;
        }

        while (!resolves.empty())
        {
            auto resolve = resolves.back();
            if (resolve.target != nullptr)
            {
                *resolve.target = objectIter->second;
            }
            if (resolve.callback)
            {
                resolve.callback(objectIter->second);
            }
            resolves.pop_back();
        }
    }

    return true;
}
} // namespace ModelArtifact
