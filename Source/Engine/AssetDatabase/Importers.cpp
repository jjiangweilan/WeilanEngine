#include "Importers.hpp"
#include "Asset/Material.hpp"
#include "Core/Asset.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Core/Texture.hpp"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

void GetGLBData(
    const std::filesystem::path& path,
    std::vector<uint32_t>& fullData,
    nlohmann::json& jsonData,
    unsigned char*& binaryData
);

std::unique_ptr<GameObject> CreateGameObjectFromNode(
    nlohmann::json& j,
    int nodeIndex,
    std::unordered_map<int, Mesh*> meshes,
    std::unordered_map<int, Material*> materials
);

std::size_t WriteAccessorDataToBuffer(
    nlohmann::json& j, unsigned char* dstBuffer, std::size_t dstOffset, unsigned char* srcBuffer, int accessorIndex
);

void SetAssetNameAndUUID(Asset* resource, nlohmann::json& j, const std::string& assetGroupName, int index);
Submesh ExtractPrimitive(nlohmann::json& j, unsigned char* binaryData, int meshIndex, int primitiveIndex);

std::unique_ptr<Model> Importers::GLB(const char* cpath, Shader* shader)
{
    // read uuid file
    std::filesystem::path path(cpath);

    std::vector<uint32_t> fullData;
    nlohmann::json jsonData;
    unsigned char* binaryData;
    GetGLBData(path, fullData, jsonData, binaryData);

    // extract mesh and submeshes
    int meshesSize = jsonData["meshes"].size();
    std::unordered_map<int, Mesh*> toOurMesh;
    std::vector<std::unique_ptr<Mesh>> meshes;
    for (int i = 0; i < meshesSize; ++i)
    {
        std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
        SetAssetNameAndUUID(mesh.get(), jsonData, "meshes", i);

        std::vector<Submesh> submeshes;
        int primitiveSize = jsonData["meshes"][i]["primitives"].size();
        for (int j = 0; j < primitiveSize; ++j)
        {
            submeshes.push_back(ExtractPrimitive(jsonData, binaryData, i, j));
        }
        mesh->SetSubmeshes(std::move(submeshes));

        toOurMesh[i] = mesh.get();
        meshes.push_back(std::move(mesh));
    }

    // extract textures
    std::unordered_map<int, Texture*> toOurTexture;
    std::vector<std::unique_ptr<Texture>> textures;
    int textureSize = jsonData["images"].size();
    for (int i = 0; i < textureSize; ++i)
    {
        int bufferViewIndex = jsonData["images"][i]["bufferView"];
        nlohmann::json& bufferViewJson = jsonData["bufferViews"][bufferViewIndex];
        int byteLength = bufferViewJson["byteLength"];
        int byteOffset = bufferViewJson["byteOffset"];
        auto tex = std::make_unique<Texture>(binaryData + byteOffset, byteLength, ImageDataType::StbSupported);

        SetAssetNameAndUUID(tex.get(), jsonData, "images", i);

        toOurTexture[i] = tex.get();
        textures.push_back(std::move(tex));
    }

    // extract materials
    std::vector<std::unique_ptr<Material>> materials;
    std::unordered_map<int, Material*> toOurMaterial;
    int materialSize = jsonData["materials"].size();
    nlohmann::json& texJson = jsonData["textures"];
    for (int i = 0; i < materialSize; ++i)
    {
        std::unique_ptr<Material> mat = std::make_unique<Material>();
        SetAssetNameAndUUID(mat.get(), jsonData, "materials", i);

        if (shader)
            mat->SetShader(shader);

        nlohmann::json& matJson = jsonData["materials"][i];
        auto config = mat->GetShaderConfig();
        config.cullMode = matJson.value("doubleSided", false) ? Gfx::CullMode::None : Gfx::CullMode::Back;
        mat->SetShaderConfig(config);

        // baseColorFactor
        std::array<float, 3> baseColorFactor =
            matJson["pbrMetallicRoughness"].value("baseColorFactor", std::array<float, 3>{1, 1, 1});
        mat->SetVector("PBR", "baseColorFactor", {baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], 1});
        mat->SetFloat("PBR", "roughness", 1.0);
        mat->SetFloat("PBR", "metallic", 1.0);

        // baseColorTex
        if (matJson["pbrMetallicRoughness"].contains("baseColorTexture"))
        {
            int baseColorSamplerIndex = matJson["pbrMetallicRoughness"]["baseColorTexture"].value("index", -1);
            if (baseColorSamplerIndex != -1)
            {
                int baseColorImageIndex = texJson[baseColorSamplerIndex]["source"];
                mat->SetTexture("baseColorTex", toOurTexture[baseColorImageIndex]);
            }
        }

        // normal map
        if (matJson.contains("normalTexture"))
        {
            int normalTextureSamplerIndex = matJson["normalTexture"].value("index", -1);
            if (normalTextureSamplerIndex != -1)
            {
                int normalTextureImageInex = texJson[normalTextureSamplerIndex]["source"];
                mat->SetTexture("normalMap", toOurTexture[normalTextureImageInex]);
            }
        }
        else
        {
            // TODO: disable shader feature for normal texture
        }

        // emission
        if (matJson.contains("emissiveTexture"))
        {
            int textureSamplerIndex = matJson["emissiveTexture"].value("index", -1);
            if (textureSamplerIndex != -1)
            {
                int emissiveTextureImageIndex = texJson[textureSamplerIndex]["source"];
                mat->SetTexture("emissiveMap", toOurTexture[emissiveTextureImageIndex]);
            }
        }

        // metallicRoughnessMap
        if (matJson["pbrMetallicRoughness"].contains("metallicRoughnessTexture"))
        {
            int metallicRougnessSamplerIndex =
                matJson["pbrMetallicRoughness"]["metallicRoughnessTexture"].value("index", -1);
            if (metallicRougnessSamplerIndex != -1)
            {
                int metallicRoughnessImageIndex = texJson[metallicRougnessSamplerIndex]["source"];
                mat->SetTexture("metallicRoughnessMap", toOurTexture[metallicRoughnessImageIndex]);
            }
        }

        toOurMaterial[i] = mat.get();
        materials.push_back(std::move(mat));
    }

    // create game objects that are presented in glb file
    nlohmann::json& scenesJson = jsonData["scenes"];
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<GameObject*> rootGameObjects;
    for (int i = 0; i < scenesJson.size(); ++i)
    {
        nlohmann::json& sceneJson = scenesJson[i];
        std::unique_ptr<GameObject> rootGameObject = MakeUnique<GameObject>();
        SetAssetNameAndUUID(rootGameObject.get(), jsonData, "scenes", i);
        rootGameObject->SetName(std::string(sceneJson["name"]));

        for (int nodeIndex : sceneJson["nodes"])
        {
            auto gameObject = CreateGameObjectFromNode(jsonData, nodeIndex, toOurMesh, toOurMaterial);
            gameObject->SetParent(rootGameObject.get());
            gameObjects.push_back(std::move(gameObject));
        }

        rootGameObjects.push_back(rootGameObject.get());
        gameObjects.push_back(std::move(rootGameObject));
    }
    auto model = std::make_unique<Model>(
        std::move(rootGameObjects),
        std::move(gameObjects),
        std::move(meshes),
        std::move(textures),
        std::move(materials),
        UUID()
    );

    return model;
}

void GetGLBData(
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

void SetAssetNameAndUUID(Asset* asset, nlohmann::json& j, const std::string& assetGroupName, int index)
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

Submesh ExtractPrimitive(nlohmann::json& j, unsigned char* binaryData, int meshIndex, int primitiveIndex)
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
    std::unique_ptr<unsigned char> vertexBuffer = std::unique_ptr<unsigned char>(new unsigned char[vertexBufferSize]);
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

    std::size_t dstOffset = 0;
    ATTRIBUTE_WRITE(POSITION);
    ATTRIBUTE_WRITE(NORMAL);
    ATTRIBUTE_WRITE(TANGENT);
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
    std::unique_ptr<unsigned char> indexBuffer = std::unique_ptr<unsigned char>(new unsigned char[indexBufferSize]);
    memcpy(indexBuffer.get(), binaryData + indexBufferOffset, indexBufferSize);

    return Submesh(std::move(vertexBuffer), std::move(bindings), std::move(indexBuffer), indexBufferType, indexCount);
}

std::unique_ptr<GameObject> CreateGameObjectFromNode(
    nlohmann::json& j,
    int nodeIndex,
    std::unordered_map<int, Mesh*> meshes,
    std::unordered_map<int, Material*> materials
)
{
    nlohmann::json& nodeJson = j["nodes"][nodeIndex];
    auto gameObject = MakeUnique<GameObject>();

    // name
    gameObject->SetName(nodeJson.value("name", "A GameObject"));

    // TRS
    std::array<float, 3> position = nodeJson.value("translation", std::array<float, 3>{0, 0, 0});
    std::array<float, 4> rotation = nodeJson.value("rotation", std::array<float, 4>{1, 0, 0, 0});
    std::array<float, 3> scale = nodeJson.value("scale", std::array<float, 3>{1, 1, 1});

    gameObject->SetPosition({position[0], position[1], position[2]});
    gameObject->SetRotation({rotation[0], rotation[1], rotation[2], rotation[3]});
    gameObject->SetScale({scale[0], scale[1], scale[2]});

    // mesh renderer
    if (nodeJson.contains("mesh"))
    {
        auto meshRenderer = gameObject->AddComponent<MeshRenderer>();
        int meshIndex = nodeJson["mesh"];
        meshRenderer->SetMesh(meshes[meshIndex]);
        int primitiveSize = j["meshes"][meshIndex]["primitives"].size();
        std::vector<Material*> mats;
        for (int i = 0; i < primitiveSize; ++i)
        {
            mats.push_back(materials[i]);
            meshRenderer->SetMaterials(mats);
        }
    }

    return gameObject;
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
