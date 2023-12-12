#include "Model.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Libs/GLB.hpp"
#include <fstream>

namespace Engine
{
DEFINE_ASSET(Model, "F675BB06-829E-43B4-BF53-F9518C7A94DB", "glb");

static std::unique_ptr<GameObject> CreateGameObjectFromNode(
    nlohmann::json& j,
    int nodeIndex,
    std::unordered_map<int, Mesh*> meshes,
    std::unordered_map<int, Material*> materials
);

static std::size_t WriteAccessorDataToBuffer(
    nlohmann::json& j, unsigned char* dstBuffer, std::size_t dstOffset, unsigned char* srcBuffer, int accessorIndex
);

static std::unique_ptr<GameObject> CreateGameObjectFromNode(
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

static std::size_t WriteAccessorDataToBuffer(
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

bool Model::LoadFromFile(const char* cpath)
{
    std::filesystem::path path(cpath);

    std::vector<uint32_t> fullData;
    unsigned char* binaryData;
    Utils::GLB::GetGLBData(path, fullData, jsonData, binaryData);

    // extract mesh and submeshes
    toOurMesh.clear();

    meshes = Utils::GLB::ExtractMeshes(jsonData, binaryData);
    int i = 0;
    for (auto& mesh : meshes)
    {
        toOurMesh[i] = mesh.get();
        i += 1;
    }

    // extract textures
    std::unordered_map<int, Texture*> toOurTexture;
    textures.clear();
    int textureSize = jsonData["images"].size();
    for (int i = 0; i < textureSize; ++i)
    {
        int bufferViewIndex = jsonData["images"][i]["bufferView"];
        nlohmann::json& bufferViewJson = jsonData["bufferViews"][bufferViewIndex];
        int byteLength = bufferViewJson["byteLength"];
        int byteOffset = bufferViewJson["byteOffset"];
        auto tex = std::make_unique<Texture>(binaryData + byteOffset, byteLength, ImageDataType::StbSupported, UUID{});
        Utils::GLB::SetAssetName(tex.get(), jsonData, "images", i);

        toOurTexture[i] = tex.get();
        textures.push_back(std::move(tex));
    }

    // extract materials
    materials.clear();
    toOurMaterial.clear();
    int materialSize = jsonData["materials"].size();
    nlohmann::json& texJson = jsonData["textures"];
    for (int i = 0; i < materialSize; ++i)
    {
        std::unique_ptr<Material> mat = std::make_unique<Material>();
        mat->SetShader(Shader::GetDefault());
        Utils::GLB::SetAssetName(mat.get(), jsonData, "materials", i);

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

    return true;
}

std::vector<std::unique_ptr<GameObject>> Model::CreateGameObject()
{
    // create game objects that are presented in glb file
    nlohmann::json& scenesJson = jsonData["scenes"];
    std::vector<GameObject*> rootGameObjects;
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    for (int i = 0; i < scenesJson.size(); ++i)
    {
        nlohmann::json& sceneJson = scenesJson[i];
        std::unique_ptr<GameObject> rootGameObject = MakeUnique<GameObject>();
        Utils::GLB::SetAssetName(rootGameObject.get(), jsonData, "scenes", i);
        rootGameObject->SetName(std::string(sceneJson["name"]));

        for (int nodeIndex : sceneJson["nodes"])
        {
            auto gameObject = CreateGameObjectFromNode(jsonData, nodeIndex, toOurMesh, toOurMaterial);
            gameObject->GetTransform()->SetParent(rootGameObject->GetTransform());
            gameObjects.push_back(std::move(gameObject));
        }

        rootGameObjects.push_back(rootGameObject.get());
        gameObjects.push_back(std::move(rootGameObject));
    }

    std::unique_ptr<GameObject> root = MakeUnique<GameObject>();
    for (auto r : rootGameObjects)
    {
        r->GetTransform()->SetParent(root->GetTransform());
    }

    gameObjects.insert(gameObjects.begin(), std::move(root));

    return gameObjects;
}

std::vector<Asset*> Model::GetInternalAssets()
{
    std::vector<Asset*> assets(meshes.size() + textures.size() + materials.size());

    int i = 0;
    for (auto& obj : meshes)
    {
        assets[i++] = obj.get();
    }

    for (auto& obj : textures)
    {
        assets[i++] = obj.get();
    }

    for (auto& obj : materials)
    {
        assets[i++] = obj.get();
    }

    return assets;
}

} // namespace Engine
