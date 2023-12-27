#include "GLB.hpp"
#include <fstream>

namespace Utils
{

static std::size_t WriteAccessorDataToBuffer(
    nlohmann::json& j, unsigned char* dstBuffer, std::size_t dstOffset, unsigned char* srcBuffer, int accessorIndex
);

static Submesh ExtractPrimitive(nlohmann::json& j, unsigned char* binaryData, int meshIndex, int primitiveIndex);
#define ATTRIBUTE_WRITE(attrName)                                                                                      \
    int index##attrName = primitiveJson["attributes"].value(#attrName, -1);                                            \
    if (index##attrName != -1)                                                                                         \
    {                                                                                                                  \
        bindings.push_back({dstOffset, 0});                                                                            \
        std::size_t byteLength =                                                                                       \
            WriteAccessorDataToBuffer(j, vertexBuffer.get(), dstOffset, binaryData, index##attrName);                  \
        dstOffset += byteLength;                                                                                       \
        bindings.back().byteSize = byteLength;                                                                         \
    }

void GLB::GetGLBData(
    const std::filesystem::path& path,
    std::vector<uint32_t>& fullData,
    nlohmann::json& jsonData,
    unsigned char*& binaryData
)
{
    uint32_t magic;
    uint32_t version;
    uint32_t length;

    std::ifstream gltf;
    gltf.open(path, std::ios_base::binary);
    uint32_t header[3];
    gltf.read((char*)header, 12);
    magic = header[0];
    version = header[1];
    length = header[2];

    assert(magic == 0x46546C67);
    assert(version == 2);

    fullData.resize(length / sizeof(uint32_t));
    gltf.seekg(std::ios_base::beg);
    gltf.read((char*)fullData.data(), length);

    // load json
    uint32_t jsonChunkLength = fullData[3];
    uint32_t jsonChunkType = fullData[4];
    assert(jsonChunkType == 0x4E4F534A);
    char* jsonChunkData = (char*)(fullData.data() + 5);

    // remove chunk padding
    for (uint32_t i = jsonChunkLength - 1; i >= 0; --i)
    {
        if (jsonChunkData[i] != ' ')
        {
            jsonChunkData[i + 1] = '\0';
            break;
        }
    }

    uint32_t bufOffset = 5 + jsonChunkLength / sizeof(uint32_t);
    uint32_t bufChunkType = fullData[bufOffset + 1];
    assert(bufChunkType == 0x004E4942);
    binaryData = (unsigned char*)(fullData.data() + bufOffset + 2);

    jsonData = nlohmann::json::parse(jsonChunkData);
    assert(
        jsonData["buffer"].contains("uri") == false &&
        "we only process valid .glb files, which shouldn't have a defined uri according to specs"
    );
}

void GLB::SetAssetName(Asset* asset, nlohmann::json& j, const std::string& assetGroupName, int index)
{
    auto& meshJson = j[assetGroupName][index];

    if (meshJson.contains("name"))
    {
        std::string meshName = meshJson["name"];
        asset->SetName(meshName);
    }
    else
    {
        asset->SetName(assetGroupName + std::to_string(index));
    }
}

std::vector<std::unique_ptr<Mesh>> GLB::ExtractMeshes(
    nlohmann::json& jsonData, unsigned char*& binaryData, int maximumMesh
)
{
    std::vector<std::unique_ptr<Mesh>> meshes;
    int meshesSize = jsonData["meshes"].size();
    for (int i = 0; i < meshesSize && i < maximumMesh; ++i)
    {
        std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
        SetAssetName(mesh.get(), jsonData, "meshes", i);

        std::vector<Submesh> submeshes;
        int primitiveSize = jsonData["meshes"][i]["primitives"].size();
        for (int j = 0; j < primitiveSize; ++j)
        {
            submeshes.push_back(ExtractPrimitive(jsonData, binaryData, i, j));
        }
        mesh->SetSubmeshes(std::move(submeshes));
        meshes.push_back(std::move(mesh));
    }

    return meshes;
}

Submesh ExtractPrimitive(nlohmann::json& j, unsigned char* binaryData, int meshIndex, int primitiveIndex)
{
    auto& meshJson = j["meshes"][meshIndex];
    auto& primitiveJson = meshJson["primitives"][primitiveIndex];

    // get vertex buffer size
    uint32_t attributesSize = 0;
    for (auto& val : primitiveJson["attributes"].items())
    {
        if (val.key() != "POSITION")
        {
            int accessorIndex = val.value();
            int bufferViewIndex = j["accessors"][accessorIndex]["bufferView"];
            int byteLength = j["bufferViews"][bufferViewIndex]["byteLength"];
            attributesSize += byteLength;
        }
    }

    //     // vertexBuffer
    //     std::vector<VertexBinding> bindings;
    //     std::unique_ptr<unsigned char> vertexBuffer = std::unique_ptr<unsigned char>(new unsigned
    //     char[vertexBufferSize]);
    // #define ATTRIBUTE_WRITE(attrName) \
//     int index##attrName = primitiveJson["attributes"].value(#attrName, -1); \
//     if (index##attrName != -1) \
//     { \
//         bindings.push_back({dstOffset, 0}); \
//         std::size_t byteLength = \
//             WriteAccessorDataToBuffer(j, vertexBuffer.get(), dstOffset, binaryData, index##attrName); \
//         dstOffset += byteLength; \
//         bindings.back().byteSize = byteLength; \
//     }
    //
    //     std::size_t dstOffset = 0;
    //     ATTRIBUTE_WRITE(POSITION);
    //     ATTRIBUTE_WRITE(NORMAL);
    //     ATTRIBUTE_WRITE(TANGENT);
    //     ATTRIBUTE_WRITE(TEXCOORD_0);

    // this is using Submesh API version 0.1, we will use Submesh API version 0.2 now
    // return Submesh(std::move(vertexBuffer), std::move(bindings), std::move(indexBuffer), indexBufferType,
    // indexCount);
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    std::vector<uint8_t> vertAttrs;
    Submesh submesh;

    // get index buffer size and count
    int indicesAccessorIndex = primitiveJson["indices"];
    int indicesBufferViewIndex = j["accessors"][indicesAccessorIndex]["bufferView"];
    auto& bufViews = j["bufferViews"];
    auto& bufView = bufViews[indicesBufferViewIndex];
    int indexBufferSize = bufView.value("byteLength", 0);
    int indexBufferOffset = bufView.value("byteOffset", 0);
    int indexCount = j["accessors"][indicesAccessorIndex]["count"];
    int indexBufferComponentType = j["accessors"][indicesAccessorIndex]["componentType"];

    Gfx::IndexBufferType indexBufferType =
        indexBufferComponentType == 5123 ? Gfx::IndexBufferType::UInt16 : Gfx::IndexBufferType::UInt32;

    // indexBuffer
    indices.resize(indexCount);
    if (indexBufferType == Gfx::IndexBufferType::UInt32)
        memcpy(indices.data(), binaryData + indexBufferOffset, indexBufferSize);
    else
    {
        for (int i = 0; i < indexCount; ++i)
        {
            indices[i] = *((uint16_t*)(binaryData + indexBufferOffset) + i);
        }
    }

    // get position data and aabb
    VertexAttribute attribute;
    std::vector<uint8_t> attributeData(attributesSize);
    size_t attributeOffset = 0;
    size_t attributeStride = 0;
    for (auto& attr : primitiveJson["attributes"].items())
    {
        std::string attributeName = attr.key();
        size_t accessorIndex = attr.value();
        auto& accessor = j["accessors"][accessorIndex];
        size_t count = accessor.value("count", 0);
        int bufferViewIndex = accessor["bufferView"];
        auto& bufferView = j["bufferViews"][bufferViewIndex];
        int byteLength = bufferView["byteLength"];
        if (attributeName != "POSITION")
        {
            attributeStride += byteLength / count;
        }
    }

    // process position
    {
        size_t accessorIndex = primitiveJson["attributes"]["POSITION"];
        auto& accessor = j["accessors"][accessorIndex];
        size_t count = accessor.value("count", 0);
        int bufferViewIndex = accessor["bufferView"];
        auto& bufferView = j["bufferViews"][bufferViewIndex];
        int byteLength = bufferView["byteLength"];
        int byteOffset = bufferView["byteOffset"];
        if (accessorIndex != -1)
        {
            glm::vec3 min = {accessor["min"][0], accessor["min"][1], accessor["min"][2]};
            glm::vec3 max = {accessor["max"][0], accessor["max"][1], accessor["max"][2]};

            submesh.SetAABB(AABB{min, max});

            if (count == 0)
                throw std::logic_error("mesh's position shouldn't be zero");

            positions.resize(count);

            if (bufferView.contains("byteStride"))
            {
                size_t byteStride = bufferView["byteStride"];
                for (size_t i = 0; i < count; ++i)
                {
                    positions[i] = *((glm::vec3*)(binaryData + byteOffset + byteStride * count));
                }
            }
            else
            {
                memcpy(positions.data(), binaryData + byteOffset, byteLength);
            }
        }
    }

    for (auto attributeName : std::vector<std::string>{
             "NORMAL",     "TANGENT",    "TEXCOORD_0", "TEXCOORD_1", "TEXCOORD_2", "TEXCOORD_3", "TEXCOORD_4",
             "TEXCOORD_5", "TEXCOORD_6", "TEXCOORD_7", "COLOR_0",    "COLOR_1",    "COLOR_2",    "COLOR_3",
             "COLOR_4",    "COLOR_5",    "COLOR_6",    "COLOR_7",    "JOINTS_0",   "JOINTS_1",   "JOINTS_2",
             "JOINTS_3",   "JOINTS_4",   "JOINTS_5",   "JOINTS_6",   "JOINTS_7",   "WEIGHTS_0",  "WEIGHTS_1",
             "WEIGHTS_2",  "WEIGHTS_3",  "WEIGHTS_4",  "WEIGHTS_5",  "WEIGHTS_6",  "WEIGHTS_7",
         })
    {
        if (primitiveJson["attributes"].contains(attributeName))
        {
            size_t accessorIndex = primitiveJson["attributes"][attributeName];
            auto& accessor = j["accessors"][accessorIndex];
            size_t count = accessor.value("count", 0);
            int bufferViewIndex = accessor["bufferView"];
            auto& bufferView = j["bufferViews"][bufferViewIndex];
            int byteLength = bufferView["byteLength"];
            int byteOffset = bufferView["byteOffset"];
            size_t byteSize = byteLength / count;

            attribute.AddAttribute(attributeName.data(), byteSize);
            // copy as interleaved data
            for (int i = 0; i < count; ++i)
            {
                memcpy(
                    attributeData.data() + attributeOffset + i * attributeStride,
                    binaryData + byteOffset + i * byteSize,
                    byteSize
                );
            }

            attributeOffset += byteSize;
        }
    }
    attribute.SetData(std::move(attributeData));

    // get other attributes

    submesh.SetPositions(std::move(positions));
    submesh.SetIndices(std::move(indices));
    submesh.SetVertexAttribute(std::move(attribute));
    submesh.Apply();

    return submesh;
}

std::size_t WriteAccessorDataToBuffer(
    nlohmann::json& j, unsigned char* dstBuffer, std::size_t dstOffset, unsigned char* srcBuffer, int accessorIndex
)
{
    int bufferViewIndex = j["accessors"][accessorIndex]["bufferView"];
    auto& bufferView = j["bufferViews"][bufferViewIndex];
    int byteLength = bufferView["byteLength"];
    int byteOffset = bufferView["byteOffset"];

    memcpy(dstBuffer + dstOffset, srcBuffer + byteOffset, byteLength);

    return byteLength;
}
} // namespace Utils
