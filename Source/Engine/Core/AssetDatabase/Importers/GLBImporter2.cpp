#pragma once
#include "GLBImporter2.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
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

UniPtr<Submesh> ExtractPrimitive(nlohmann::json& j, unsigned char* binaryData, int meshIndex, int primitiveIndex)
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
    UniPtr<unsigned char> indexBuffer = UniPtr<unsigned char>(new unsigned char[indexBufferSize]);
    memcpy(indexBuffer.Get(), binaryData + indexBufferOffset, indexBufferSize);

    Submesh* mesh =
        new Submesh(std::move(vertexBuffer), std::move(bindings), std::move(indexBuffer), indexBufferType, indexCount);

    return UniPtr<Submesh>(mesh);
} // namespace Engine::Internal

// we use a different naming system, our model is mesh in gltf
UniPtr<Model2> ExtractMesh(nlohmann::json& j, uint32_t* binaryData, int meshIndex)
{
    auto& meshJson = j["meshes"][meshIndex];
    std::string name = meshJson["name"];
    return nullptr;
}

void OverrideUUIDFromDiskFile(nlohmann::json& jsonData,
                              nlohmann::json& uuidJson,
                              nlohmann::json& newUUIDJson,
                              std::string assetGroupName)
{
    int i = 0;
    bool hasGrupName = uuidJson.contains(assetGroupName);
    newUUIDJson[assetGroupName] = nlohmann::json::array();
    for (auto assetJson : jsonData[assetGroupName])
    {
        if (i < uuidJson[assetGroupName].size() && hasGrupName)
        {
            newUUIDJson[assetGroupName].push_back(uuidJson[assetGroupName][i]);
        }
        else
        {
            newUUIDJson[assetGroupName].push_back(UUID().ToString());
        }
        i += 1;
    }
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
    std::filesystem::copy(path, outputDir / "main.glb", std::filesystem::copy_options::overwrite_existing);

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

    nlohmann::json newUUIDJson = nlohmann::json::object();
    std::string assetGroupName = "meshes";
    OverrideUUIDFromDiskFile(jsonData, uuidJson, newUUIDJson, "meshes");
    OverrideUUIDFromDiskFile(jsonData, uuidJson, newUUIDJson, "images");
    OverrideUUIDFromDiskFile(jsonData, uuidJson, newUUIDJson, "materials");
    OverrideUUIDFromDiskFile(jsonData, uuidJson, newUUIDJson, "scenes");
    OverrideUUIDFromDiskFile(jsonData, uuidJson, newUUIDJson, "nodes");

    // generate uuid json file

    // write the new uuid json file to disk
    std::ofstream uuidOut(outputDir / "uuid.json", std::ios::trunc);
    uuidOut << newUUIDJson.dump();
} // namespace Engine::Internal

void SetAssetNameAndUUID(
    AssetObject* asset, nlohmann::json& j, nlohmann::json& uuidJson, const std::string& assetGroupName, int index)
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

    UUID uuid = UUID(uuidJson[assetGroupName][index]);
    asset->SetUUID(uuid);
}

UniPtr<GameObject> CreateGameObjectFromNode(nlohmann::json& j,
                                            int nodeIndex,
                                            std::unordered_map<int, Mesh2*> meshes,
                                            std::unordered_map<int, Material*> materials)
{
    nlohmann::json& nodeJson = j["nodes"][nodeIndex];
    auto gameObject = MakeUnique<GameObject>();

    // name
    gameObject->SetName(nodeJson.value("name", "A GameObject"));

    // TRS
    std::array<float, 3> position = nodeJson.value("translation", std::array<float, 3>{0, 0, 0});
    std::array<float, 4> rotation = nodeJson.value("rotation", std::array<float, 4>{1, 0, 0, 0});
    std::array<float, 3> scale = nodeJson.value("scale", std::array<float, 3>{1, 1, 1});

    gameObject->GetTransform()->SetPosition({position[0], position[1], position[2]});
    gameObject->GetTransform()->SetRotation({rotation[0], rotation[1], rotation[2], rotation[3]});
    gameObject->GetTransform()->SetScale({scale[0], scale[1], scale[2]});

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

    // extract mesh and submeshes
    int meshesSize = jsonData["meshes"].size();
    std::unordered_map<int, Mesh2*> toOurMesh;
    std::vector<UniPtr<Mesh2>> meshes;
    for (int i = 0; i < meshesSize; ++i)
    {
        UniPtr<Mesh2> mesh = MakeUnique<Mesh2>();
        SetAssetNameAndUUID(mesh.Get(), jsonData, uuidJson, "meshes", i);

        int primitiveSize = jsonData["meshes"][i]["primitives"].size();
        for (int j = 0; j < primitiveSize; ++j)
        {
            mesh->submeshes.push_back(ExtractPrimitive(jsonData, binaryData, i, j));
        }
        toOurMesh[i] = mesh.Get();
        meshes.push_back(std::move(mesh));
    }

    // extract textures
    std::unordered_map<int, Texture*> toOurTexture;
    std::vector<UniPtr<Texture>> textures;
    int textureSize = jsonData["images"].size();
    for (int i = 0; i < textureSize; ++i)
    {
        int bufferViewIndex = jsonData["images"][i]["bufferView"];
        nlohmann::json& bufferViewJson = jsonData["bufferViews"][bufferViewIndex];
        int byteLength = bufferViewJson["byteLength"];
        int byteOffset = bufferViewJson["byteOffset"];
        auto tex = LoadTextureFromBinary(binaryData + byteOffset, byteLength);
        SetAssetNameAndUUID(tex.Get(), jsonData, uuidJson, "images", i);

        toOurTexture[i] = tex.Get();
        textures.push_back(std::move(tex));
    }

    // extract materials
    std::vector<UniPtr<Material>> materials;
    std::unordered_map<int, Material*> toOurMaterial;
    int materialSize = jsonData["materials"].size();
    nlohmann::json& texJson = jsonData["textures"];
    for (int i = 0; i < materialSize; ++i)
    {
        UniPtr<Material> mat = MakeUnique<Material>();
        SetAssetNameAndUUID(mat.Get(), jsonData, uuidJson, "materials", i);

        mat->SetShader("Game/StandardPBR");

        nlohmann::json& matJson = jsonData["materials"][i];
        mat->GetShaderConfig().cullMode =
            matJson.value("doubleSided", false) ? Gfx::CullMode::None : Gfx::CullMode::Back;

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

        toOurMaterial[i] = mat.Get();
        materials.push_back(std::move(mat));
    }

    // create game objects that are presented in glb file
    nlohmann::json& scenesJson = jsonData["scenes"];
    std::vector<UniPtr<GameObject>> gameObjects;
    std::vector<GameObject*> rootGameObjects;
    for (int i = 0; i < scenesJson.size(); ++i)
    {
        nlohmann::json& sceneJson = scenesJson[i];
        UniPtr<GameObject> rootGameObject = MakeUnique<GameObject>();
        SetAssetNameAndUUID(rootGameObject.Get(), jsonData, uuidJson, "scenes", i);
        rootGameObject->SetName(sceneJson["name"]);

        for (int nodeIndex : sceneJson["nodes"])
        {
            auto gameObject = CreateGameObjectFromNode(jsonData, nodeIndex, toOurMesh, toOurMaterial);
            gameObject->GetTransform()->SetParent(rootGameObject->GetTransform());
            gameObjects.push_back(std::move(gameObject));
        }

        rootGameObjects.push_back(rootGameObject.Get());
        gameObjects.push_back(std::move(rootGameObject));
    }
    auto model = MakeUnique<Model2>(std::move(rootGameObjects),
                                    std::move(gameObjects),
                                    std::move(meshes),
                                    std::move(textures),
                                    std::move(materials),
                                    uuid);
    return model;
}
} // namespace Engine::Internal
