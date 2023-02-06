#pragma once
#include "GLBImporter2.hpp"
#include "Core/Texture.hpp"
#include <fstream>
#include <iterator>
namespace Engine::Internal
{

void GetGLBData(const std::filesystem::path& path,
                std::vector<uint32_t>& fullData,
                nlohmann::json& jsonData,
                unsigned char*& binaryData)
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
    assert(jsonData["buffer"].contains("uri") == false &&
           "we only process valid .glb files, which shouldn't have a defined uri according to specs");
}

std::size_t WriteAccessorDataToBuffer(
    nlohmann::json& j, unsigned char* dstBuffer, std::size_t dstOffset, unsigned char* srcBuffer, int accessorIndex)
{
    int bufferViewIndex = j["accessors"][accessorIndex]["bufferView"];
    auto& bufferView = j["bufferViews"][bufferViewIndex];
    int byteLength = bufferView["byteLength"];
    int byteOffset = bufferView["byteOffset"];

    memcpy(dstBuffer + dstOffset, srcBuffer + byteOffset, byteLength);

    return byteLength;
}

UniPtr<Mesh2> ExtractPrimitive(
    nlohmann::json& j, nlohmann::json& uuidJson, unsigned char* binaryData, int meshIndex, int primitiveIndex)
{
    auto& meshJson = j["meshes"][meshIndex];
    auto& primitiveJson = meshJson["primitives"][primitiveIndex];

    // get vertex buffer size
    uint32_t vertexBufferSize = 0;
    for (auto& val : primitiveJson["attributes"].items())
    {
        int accessorIndex = val.value();
        int bufferViewIndex = j["accessors"][accessorIndex]["bufferView"];
        int byteLength = j["bufferViews"][bufferViewIndex]["byteLength"];
        vertexBufferSize += byteLength;
    }

    // vertexBuffer
    std::vector<VertexBinding> bindings;
    UniPtr<unsigned char> vertexBuffer = UniPtr<unsigned char>(new unsigned char[vertexBufferSize]);
#define ATTRIBUTE_WRITE(attrName)                                                                                      \
    int index##attrName = primitiveJson["attributes"].value(#attrName, -1);                                            \
    if (index##attrName != -1)                                                                                         \
    {                                                                                                                  \
        bindings.push_back({dstOffset, 0});                                                                            \
        std::size_t byteLength =                                                                                       \
            WriteAccessorDataToBuffer(j, vertexBuffer.Get(), dstOffset, binaryData, index##attrName);                  \
        dstOffset += byteLength;                                                                                       \
        bindings.back().byteSize = byteLength;                                                                         \
    }

    std::size_t dstOffset = 0;
    ATTRIBUTE_WRITE(POSITION);
    ATTRIBUTE_WRITE(NORMAL);
    ATTRIBUTE_WRITE(TEXCOORD_0);

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
    UniPtr<unsigned char> indexBuffer = UniPtr<unsigned char>(new unsigned char[indexBufferSize]);
    memcpy(indexBuffer.Get(), binaryData + indexBufferOffset, indexBufferSize);

    Mesh2* mesh =
        new Mesh2(std::move(vertexBuffer), std::move(bindings), std::move(indexBuffer), indexBufferType, indexCount);

    std::string meshName = meshJson["name"];
    UUID uuid = uuidJson.value(meshName, UUID::empty.ToString());
    mesh->SetName(meshName);
    mesh->SetUUID(uuid);
    return UniPtr<Mesh2>(mesh);
} // namespace Engine::Internal

// we use a different naming system, our model is mesh in gltf
UniPtr<Model2> ExtractMesh(nlohmann::json& j, uint32_t* binaryData, int meshIndex)
{
    auto& meshJson = j["meshes"][meshIndex];
    std::string name = meshJson["name"];
    return nullptr;
}

void GLBImporter2::Import(const std::filesystem::path& path,
                          const std::filesystem::path& root,
                          const nlohmann::json& json,
                          const UUID& rootUUID,
                          const std::unordered_map<std::string, UUID>& containedUUIDs)
{
    // create a folder in library path
    auto outputDir = root / "Library" / rootUUID.ToString();
    if (!std::filesystem::exists(outputDir))
    {
        std::filesystem::create_directory(outputDir);
    }

    // copy the original file to library
    std::filesystem::copy(path, outputDir / "main.glb");

    // get glb json object
    std::vector<uint32_t> fullData;
    nlohmann::json jsonData;
    unsigned char* binaryData;
    GetGLBData(outputDir / "main.glb", fullData, jsonData, binaryData);

    // get the uuid json file
    std::ifstream jsonFile(outputDir / "uuid.json");
    nlohmann::json uuidJson = nlohmann::json::object();
    if (jsonFile.good() && jsonFile.is_open())
    {
        std::vector<unsigned char> uuidJsonBinary(std::istreambuf_iterator<char>(jsonFile), {});
        uuidJson = nlohmann::json::parse(uuidJsonBinary);
    }

    // generate uuid json file
    nlohmann::json newUUIDJson = nlohmann::json::object();
    for (auto meshJson : jsonData["meshes"])
    {
        std::string name = meshJson["name"];
        // if uuid is already generated, then use it. Or we create a new uuid
        newUUIDJson[name] = uuidJson.value(name, UUID().ToString());
    }

    // write the new uuid json file to disk
    std::ofstream uuidOut(outputDir / "uuid.json", std::ios::trunc);
    uuidOut << newUUIDJson.dump();
}

UniPtr<AssetObject> GLBImporter2::Load(const std::filesystem::path& root,
                                       ReferenceResolver& refResolver,
                                       const UUID& uuid)
{
    std::filesystem::path dir = root / "Library" / uuid.ToString();
    std::filesystem::path glbPath = dir / "main.glb";
    std::filesystem::path uuidPath = dir / "uuid.json";

    // read uuid file
    nlohmann::json uuidJson = nlohmann::json::object();
    if (std::filesystem::exists(uuidPath))
    {
        std::ifstream uuidFile(uuidPath);
        uuidJson = nlohmann::json::parse(uuidFile);
    }

    std::vector<uint32_t> fullData;
    nlohmann::json jsonData;
    unsigned char* binaryData;
    GetGLBData(glbPath, fullData, jsonData, binaryData);

    // extract meshes
    std::vector<UniPtr<Mesh2>> meshes;
    int meshesSize = jsonData["meshes"].size();
    for (int i = 0; i < meshesSize; ++i)
    {
        int primitiveSize = jsonData["meshes"][i]["primitives"].size();
        for (int j = 0; j < primitiveSize; ++j)
        {
            meshes.push_back(ExtractPrimitive(jsonData, uuidJson, binaryData, i, j));
        }
    }

    return MakeUnique<Model2>(std::move(meshes), uuid);
}
} // namespace Engine::Internal
