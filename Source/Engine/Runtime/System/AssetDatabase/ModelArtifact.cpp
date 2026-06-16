#include "ModelArtifact.hpp"
#include "Engine/Driver/GfxDriver/VertexAttributes.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <limits>

namespace ModelArtifact
{
namespace
{
constexpr uint32_t MeshMagic = 0x4D534842;      // MSHB
constexpr uint32_t AnimationClipMagic = 0x41434C50; // ACLP
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

    BlobHeader blobHeader;
    std::memcpy(&blobHeader, data.data(), sizeof(blobHeader));
    if (blobHeader.magic != expectedMagic || blobHeader.version != BlobVersion)
    {
        return false;
    }

    constexpr size_t headerOffset = sizeof(BlobHeader);
    if (blobHeader.jsonSize > data.size() - headerOffset)
    {
        return false;
    }

    size_t payloadOffset = headerOffset + static_cast<size_t>(blobHeader.jsonSize);
    header = nlohmann::json::parse(data.data() + headerOffset, data.data() + payloadOffset, nullptr, false);
    if (header.is_discarded() || !header.is_object())
    {
        return false;
    }

    payload = data.data() + payloadOffset;
    payloadSize = data.size() - payloadOffset;
    return true;
}

bool TryMultiply(size_t left, size_t right, size_t& result)
{
    if (left != 0 && right > std::numeric_limits<size_t>::max() / left)
    {
        return false;
    }

    result = left * right;
    return true;
}

const uint8_t* ReadRange(const uint8_t* payload, size_t payloadSize, size_t offset, size_t size)
{
    if (offset > payloadSize || size > payloadSize - offset)
    {
        return nullptr;
    }
    return payload + offset;
}

bool ReadSize(const nlohmann::json& object, const char* key, size_t& value)
{
    auto iter = object.find(key);
    if (iter == object.end())
    {
        return false;
    }

    uint64_t parsed = 0;
    if (iter->is_number_unsigned())
    {
        parsed = iter->get<uint64_t>();
    }
    else if (iter->is_number_integer())
    {
        int64_t signedValue = iter->get<int64_t>();
        if (signedValue < 0)
        {
            return false;
        }
        parsed = static_cast<uint64_t>(signedValue);
    }
    else
    {
        return false;
    }

    if (parsed > std::numeric_limits<size_t>::max())
    {
        return false;
    }
    value = static_cast<size_t>(parsed);
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

bool VectorToMat4(const nlohmann::json& values, glm::mat4& matrix)
{
    if (!values.is_array() || values.size() != 16)
    {
        return false;
    }

    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            matrix[c][r] = values[c * 4 + r].get<float>();
        }
    }
    return true;
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
const uint8_t* ReadKeyRange(const uint8_t* payload, size_t payloadSize, size_t offset, size_t count)
{
    size_t size = 0;
    if (!TryMultiply(count, sizeof(KeyType), size))
    {
        return nullptr;
    }
    return ReadRange(payload, payloadSize, offset, size);
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

    try
    {
        size_t vertexCount = 0;
        size_t indexCount = 0;
        size_t positionsOffset = 0;
        size_t positionsSize = 0;
        size_t attributesOffset = 0;
        size_t attributesSize = 0;
        size_t indicesOffset = 0;
        size_t indicesSize = 0;
        if (!ReadSize(header, "vertexCount", vertexCount) ||
            !ReadSize(header, "indexCount", indexCount) ||
            !ReadSize(header, "positionsOffset", positionsOffset) ||
            !ReadSize(header, "positionsSize", positionsSize) ||
            !ReadSize(header, "attributesOffset", attributesOffset) ||
            !ReadSize(header, "attributesSize", attributesSize) ||
            !ReadSize(header, "indicesOffset", indicesOffset) ||
            !ReadSize(header, "indicesSize", indicesSize))
        {
            return nullptr;
        }

        size_t expectedPositionsSize = 0;
        size_t expectedIndicesSize = 0;
        if (!TryMultiply(vertexCount, sizeof(glm::vec3), expectedPositionsSize) ||
            !TryMultiply(indexCount, sizeof(uint32_t), expectedIndicesSize) ||
            positionsSize != expectedPositionsSize ||
            indicesSize != expectedIndicesSize)
        {
            return nullptr;
        }

        nlohmann::json attributeDescriptions = header.value("attributes", nlohmann::json::array());
        if (!attributeDescriptions.is_array())
        {
            return nullptr;
        }

        size_t attributeStride = 0;
        std::vector<VertexAttributes::Attribute> parsedAttributes;
        for (const auto& attribute : attributeDescriptions)
        {
            size_t attributeSize = 0;
            if (!attribute.is_object() || !ReadSize(attribute, "size", attributeSize) ||
                attributeSize == 0 || attributeSize > std::numeric_limits<int>::max() ||
                attributeStride > std::numeric_limits<size_t>::max() - attributeSize)
            {
                return nullptr;
            }

            const std::string name = attribute.at("name").get<std::string>();
            const int64_t semantic = attribute.at("semantic").get<int64_t>();
            const int64_t semanticIndex = attribute.at("semanticIndex").get<int64_t>();
            if (semantic < 0 || semantic > static_cast<int64_t>(VertexAttributeSemantics::Bone) ||
                semanticIndex < 0 || semanticIndex > std::numeric_limits<int>::max())
            {
                return nullptr;
            }

            parsedAttributes.push_back({
                name,
                static_cast<VertexAttributeSemantics>(semantic),
                static_cast<int>(semanticIndex),
                static_cast<int>(attributeSize),
            });
            attributeStride += attributeSize;
        }

        size_t expectedAttributesSize = 0;
        if (!TryMultiply(vertexCount, attributeStride, expectedAttributesSize) ||
            attributesSize != expectedAttributesSize)
        {
            return nullptr;
        }

        const uint8_t* positionBytes = ReadRange(payload, payloadSize, positionsOffset, positionsSize);
        const uint8_t* attributeBytes = ReadRange(payload, payloadSize, attributesOffset, attributesSize);
        const uint8_t* indexBytes = ReadRange(payload, payloadSize, indicesOffset, indicesSize);
        if (positionBytes == nullptr || attributeBytes == nullptr || indexBytes == nullptr)
        {
            return nullptr;
        }

        auto aabbIter = header.find("aabb");
        if (aabbIter == header.end() || !aabbIter->is_object())
        {
            return nullptr;
        }
        auto aabbMin = aabbIter->find("min");
        auto aabbMax = aabbIter->find("max");
        if (aabbMin == aabbIter->end() || aabbMax == aabbIter->end() ||
            !aabbMin->is_array() || !aabbMax->is_array() ||
            aabbMin->size() != 3 || aabbMax->size() != 3)
        {
            return nullptr;
        }

        std::vector<glm::vec3> positions(vertexCount);
        if (positionsSize != 0)
        {
            std::memcpy(positions.data(), positionBytes, positionsSize);
        }
        std::vector<uint8_t> attributeData(attributesSize);
        if (attributesSize != 0)
        {
            std::memcpy(attributeData.data(), attributeBytes, attributesSize);
        }
        std::vector<uint32_t> indices(indexCount);
        if (indicesSize != 0)
        {
            std::memcpy(indices.data(), indexBytes, indicesSize);
        }

        VertexAttributes attributes;
        for (const auto& attribute : parsedAttributes)
        {
            attributes.AddAttribute(attribute.name.c_str(), attribute.semanticName, attribute.semanticIndex, attribute.size);
        }
        attributes.SetData(std::move(attributeData));

        Submesh submesh;
        submesh.SetPositions(std::move(positions));
        submesh.SetVertexAttribute(std::move(attributes));
        submesh.SetIndices(std::move(indices));
        AABB aabb;
        aabb.min = {aabbMin->at(0).get<float>(), aabbMin->at(1).get<float>(), aabbMin->at(2).get<float>()};
        aabb.max = {aabbMax->at(0).get<float>(), aabbMax->at(1).get<float>(), aabbMax->at(2).get<float>()};
        submesh.SetAABB(aabb);

        Skeleton skeleton;
        auto skeletonIter = header.find("skeleton");
        if (skeletonIter != header.end())
        {
            if (!skeletonIter->is_array())
            {
                return nullptr;
            }
            for (const auto& bone : *skeletonIter)
            {
                glm::mat4 offsetMatrix(1.0f);
                if (!bone.is_object() || !VectorToMat4(bone.at("offsetMatrix"), offsetMatrix))
                {
                    return nullptr;
                }
                skeleton.push_back({bone.at("name").get<std::string>(), offsetMatrix});
            }
        }

        submesh.Apply();

        auto mesh = std::make_unique<Mesh>();
        mesh->SetName(header.value("name", "Mesh"));
        std::vector<Submesh> submeshes;
        submeshes.push_back(std::move(submesh));
        mesh->SetSubmeshes(std::move(submeshes));
        mesh->SetSkeleton(std::move(skeleton));
        return mesh;
    }
    catch (const nlohmann::json::exception&)
    {
        return nullptr;
    }
}

bool WriteAnimationClipBlob(const std::filesystem::path& path, const AnimationClip& clip)
{
    nlohmann::json header;
    std::vector<uint8_t> payload;
    header["name"] = clip.GetName();
    header["tickPerSecond"] = clip.tickPerSecond;
    header["duration"] = clip.duration;

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

        header["channels"].push_back(std::move(channelJson));
    }

    return WriteBlob(path, AnimationClipMagic, header, payload);
}

std::unique_ptr<AnimationClip> ReadAnimationClipBlob(const PodVector<uint8_t>& data)
{
    nlohmann::json header;
    const uint8_t* payload = nullptr;
    size_t payloadSize = 0;
    if (!ReadBlob(data, AnimationClipMagic, header, payload, payloadSize))
    {
        return nullptr;
    }

    try
    {
        nlohmann::json channelDescriptions = header.value("channels", nlohmann::json::array());
        if (!channelDescriptions.is_array())
        {
            return nullptr;
        }

        std::vector<AnimationClip::Channel> channels;
        for (const auto& channelJson : channelDescriptions)
        {
            size_t positionsOffset = 0;
            size_t positionsCount = 0;
            size_t rotationsOffset = 0;
            size_t rotationsCount = 0;
            size_t scalingsOffset = 0;
            size_t scalingsCount = 0;
            if (!channelJson.is_object() ||
                !ReadSize(channelJson, "positionsOffset", positionsOffset) ||
                !ReadSize(channelJson, "positionsCount", positionsCount) ||
                !ReadSize(channelJson, "rotationsOffset", rotationsOffset) ||
                !ReadSize(channelJson, "rotationsCount", rotationsCount) ||
                !ReadSize(channelJson, "scalingsOffset", scalingsOffset) ||
                !ReadSize(channelJson, "scalingsCount", scalingsCount))
            {
                return nullptr;
            }

            const uint8_t* positions = ReadKeyRange<PackedVec3Key>(payload, payloadSize, positionsOffset, positionsCount);
            const uint8_t* rotations = ReadKeyRange<PackedQuatKey>(payload, payloadSize, rotationsOffset, rotationsCount);
            const uint8_t* scalings = ReadKeyRange<PackedVec3Key>(payload, payloadSize, scalingsOffset, scalingsCount);
            if (positions == nullptr || rotations == nullptr || scalings == nullptr)
            {
                return nullptr;
            }

            AnimationClip::Channel channel;
            channel.nodeName = channelJson.at("nodeName").get<std::string>();
            for (size_t i = 0; i < positionsCount; ++i)
            {
                PackedVec3Key key;
                std::memcpy(&key, positions + i * sizeof(key), sizeof(key));
                channel.positions.push_back({key.time, {key.x, key.y, key.z}});
            }
            for (size_t i = 0; i < rotationsCount; ++i)
            {
                PackedQuatKey key;
                std::memcpy(&key, rotations + i * sizeof(key), sizeof(key));
                channel.rotations.push_back({key.time, {key.w, key.x, key.y, key.z}});
            }
            for (size_t i = 0; i < scalingsCount; ++i)
            {
                PackedVec3Key key;
                std::memcpy(&key, scalings + i * sizeof(key), sizeof(key));
                channel.scalings.push_back({key.time, {key.x, key.y, key.z}});
            }

            channels.push_back(std::move(channel));
        }

        auto clip = std::make_unique<AnimationClip>();
        clip->SetName(header.at("name").get<std::string>());
        clip->tickPerSecond = header.at("tickPerSecond").get<float>();
        clip->duration = header.at("duration").get<float>();
        clip->channels = std::move(channels);
        return clip;
    }
    catch (const nlohmann::json::exception&)
    {
        return nullptr;
    }
}

bool WriteAnimationSetBlob(const std::filesystem::path& path, const AnimationSet& animationSet)
{
    JsonSerializer serializer;
    animationSet.Serialize(&serializer);
    auto binary = serializer.GetBinary();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open() || !out.good())
        return false;

    out.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
    return true;
}

std::unique_ptr<AnimationSet> ReadAnimationSetBlob(const PodVector<uint8_t>& data)
{
    std::vector<uint8_t> binary(data.data(), data.data() + data.size());
    JsonSerializer serializer(binary, nullptr);
    auto animationSet = std::make_unique<AnimationSet>();
    animationSet->Deserialize(&serializer);
    return animationSet;
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
