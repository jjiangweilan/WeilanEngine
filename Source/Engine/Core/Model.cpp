#include "Model.hpp"
#include "Core/Component/AnimationPlayer.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Libs/GLB.hpp"
#include "Libs/Math.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include <fstream>

DEFINE_ASSET(Model, "F675BB06-829E-43B4-BF53-F9518C7A94DB", "glb,fbx");

static std::size_t WriteAccessorDataToBuffer(
    nlohmann::json& j, unsigned char* dstBuffer, std::size_t dstOffset, unsigned char* srcBuffer, int accessorIndex
);

std::vector<std::unique_ptr<GameObject>> Model::CreateGameObjectFromNode(
    nlohmann::json& j,
    int nodeIndex,
    std::unordered_map<int, Mesh*>& meshes,
    GameObject* parent,
    Material* defaultMaterial
)
{
    nlohmann::json& nodeJson = j["nodes"][nodeIndex];
    auto gameObject = std::make_unique<GameObject>();

    // name
    gameObject->SetName(nodeJson.value("name", "New GameObject"));

    if (nodeJson.contains("matrix"))
    {
        std::array<float, 16> matrix =
            nodeJson.value("matrix", std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});

        glm::mat4 m = {
            matrix[0],
            matrix[1],
            matrix[2],
            matrix[3],
            matrix[4],
            matrix[5],
            matrix[6],
            matrix[7],
            matrix[8],
            matrix[9],
            matrix[10],
            matrix[11],
            matrix[12],
            matrix[13],
            matrix[14],
            matrix[15],
        };

        glm::vec3 position, scale;
        glm::quat rotation;
        Math::DecomposeMatrix(m, position, scale, rotation);
        gameObject->SetLocalPosition(position);
        gameObject->SetEulerAngles(glm::eulerAngles(rotation));
        gameObject->SetLocalScale(scale);
    }
    else
    {
        // TRS
        std::array<float, 3> position = nodeJson.value("translation", std::array<float, 3>{0, 0, 0});
        std::array<float, 4> rotation = nodeJson.value("rotation", std::array<float, 4>{0, 0, 0, 1});
        std::array<float, 3> scale = nodeJson.value("scale", std::array<float, 3>{1, 1, 1});

        gameObject->SetLocalPosition({position[0], position[1], position[2]});
        gameObject->SetEulerAngles(glm::eulerAngles(glm::quat{rotation[3], rotation[0], rotation[1], rotation[2]}));
        gameObject->SetLocalScale({scale[0], scale[1], scale[2]});
    }

    // mesh renderer
    if (nodeJson.contains("mesh"))
    {
        auto meshRenderer = gameObject->AddComponent<MeshRenderer>();
        int meshIndex = nodeJson["mesh"];
        meshRenderer->SetMesh(meshes[meshIndex]);
        auto& primitives = j["meshes"][meshIndex]["primitives"];
        int primitiveSize = primitives.size();
        std::vector<Material*> mats;
        for (int i = 0; i < primitiveSize; ++i)
        {
            auto& p = primitives[i];
            int matIndex = p.value("material", -1);
            auto matIter = toOurMaterials.find(matIndex);
            if (matIter != toOurMaterials.end())
            {
                auto mat = toOurMaterials[matIndex];
                if (mat->GetShader() == nullptr)
                {
                    mat->SetShader("SceneLit");
                }
                mats.push_back(mat);
            }
            else
            {
                // in case no material is provided in model file
                auto newMat = std::make_unique<Material>();
                Material* mat = newMat.get();
                mat->SetName(fmt::format("Auto Gen Material {}", i));
                mat->SetShader("SceneLit");
                if (p["attributes"].contains("TANGENT"))
                {
                    mat->EnableFeature("_Vertex_Tangent");
                }
                if (p["attributes"].contains("TEXCOORD_0"))
                {
                    mat->EnableFeature("_Vertex_UV0");
                }
                mat->SetVector("PBR", "baseColorFactor", {0.5, 0.5, 0.5, 0.5});
                mat->SetVector("PBR", "emissive", {0, 0, 0, 0});
                mat->SetFloat("PBR", "roughness", 0.4);
                mat->SetFloat("PBR", "metallic", 0.2);
                mat->SetFloat("PBR", "alphaCutoff", 0.0f);

                toOurMaterials[matIndex] = mat;
                this->materials.push_back(std::move(newMat));
                mats.push_back(mat);
            }
        }
        meshRenderer->SetMaterials(mats);
    }

    std::vector<std::unique_ptr<GameObject>> rlt;
    auto temp = gameObject.get();
    if (parent)
        temp->SetParent(parent);
    rlt.push_back(std::move(gameObject));
    if (nodeJson.contains("children"))
    {
        for (int i : nodeJson["children"])
        {
            auto children = CreateGameObjectFromNode(j, i, meshes, temp, defaultMaterial);
            rlt.insert(rlt.end(), std::make_move_iterator(children.begin()), std::make_move_iterator(children.end()));
        }
    }

    return rlt;
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

static int GetImageIndex(nlohmann::json& texJson)
{
    if (texJson.contains("source"))
    {
        return texJson["source"];
    }

    return texJson["extensions"]["KHR_texture_basisu"]["source"];
}

//bool Model::LoadFromFile(const char* cpath)
//{
//    std::filesystem::path path(cpath);
//
//    std::vector<uint32_t> fullData;
//    unsigned char* binaryData;
//    Utils::GLB::GetGLBData(path, fullData, jsonData, binaryData);
//
//    // extract mesh and submeshes
//    toOurMesh.clear();
//
//    meshes = Utils::GLB::ExtractMeshes(jsonData, binaryData);
//    int i = 0;
//    for (auto& mesh : meshes)
//    {
//        toOurMesh[i] = mesh.get();
//        i += 1;
//    }
//
//    int materialSize = jsonData["materials"].size();
//    nlohmann::json& texJson = jsonData["textures"];
//    int textureSize = jsonData["images"].size();
//    std::vector<Gfx::GfxFormat> textureFormats(textureSize, Gfx::GfxFormat::Invalid);
//    for (int i = 0; i < materialSize; ++i)
//    {
//        nlohmann::json& matJson = jsonData["materials"][i];
//        if (matJson["pbrMetallicRoughness"].contains("baseColorTexture"))
//        {
//            int baseColorSamplerIndex = matJson["pbrMetallicRoughness"]["baseColorTexture"].value("index", -1);
//            if (baseColorSamplerIndex != -1)
//            {
//                int baseColorImageIndex = GetImageIndex(texJson[baseColorSamplerIndex]);
//                textureFormats[baseColorImageIndex] = Gfx::GfxFormat::R8G8B8A8_SRGB;
//            }
//        }
//
//        if (matJson["pbrMetallicRoughness"].contains("metallicRoughnessTexture"))
//        {
//            int metallicRougnessSamplerIndex =
//                matJson["pbrMetallicRoughness"]["metallicRoughnessTexture"].value("index", -1);
//            if (metallicRougnessSamplerIndex != -1)
//            {
//                int metallicRoughnessImageIndex = GetImageIndex(texJson[metallicRougnessSamplerIndex]);
//                textureFormats[metallicRoughnessImageIndex] = Gfx::GfxFormat::R8G8B8A8_UNorm;
//            }
//        }
//
//        if (matJson.contains("normalTexture"))
//        {
//            int normalTextureSamplerIndex = matJson["normalTexture"].value("index", -1);
//            if (normalTextureSamplerIndex != -1)
//            {
//                int normalTextureImageIndex = GetImageIndex(texJson[normalTextureSamplerIndex]);
//                textureFormats[normalTextureImageIndex] = Gfx::GfxFormat::R8G8B8A8_UNorm;
//            }
//        }
//
//        if (matJson.contains("emissiveTexture"))
//        {
//            int textureSamplerIndex = matJson["emissiveTexture"].value("index", -1);
//            if (textureSamplerIndex != -1)
//            {
//                int emissiveTextureImageIndex = GetImageIndex(texJson[textureSamplerIndex]);
//                textureFormats[emissiveTextureImageIndex] = Gfx::GfxFormat::R8G8B8A8_UNorm;
//            }
//        }
//    }
//
//    // extract textures
//    std::unordered_map<int, Texture*> toOurTexture;
//    textures.clear();
//    for (int i = 0; i < textureSize; ++i)
//    {
//        int bufferViewIndex = jsonData["images"][i]["bufferView"];
//        std::string mimeType = jsonData["images"][i]["mimeType"];
//        nlohmann::json& bufferViewJson = jsonData["bufferViews"][bufferViewIndex];
//        int byteLength = bufferViewJson["byteLength"];
//        int byteOffset = bufferViewJson.value("byteOffset", 0);
//
//        std::unique_ptr<Texture> tex = nullptr;
//        if (mimeType == "image/jpeg" || mimeType == "image/png")
//        {
//            tex = std::make_unique<Texture>(
//                binaryData + byteOffset,
//                byteLength,
//                ImageDataType::StbSupported,
//                textureFormats[i],
//                UUID{}
//            );
//        }
//        else if (mimeType == "image/ktx2")
//        {
//            tex = std::make_unique<Texture>(
//                binaryData + byteOffset,
//                byteLength,
//                ImageDataType::Ktx,
//                textureFormats[i],
//                UUID{}
//            );
//        }
//        else
//            throw std::runtime_error("unsupported mime type");
//        Utils::GLB::SetAssetName(tex.get(), jsonData, "images", i);
//
//        toOurTexture[i] = tex.get();
//        textures.push_back(std::move(tex));
//    }
//
//    // extract materials
//    materials.clear();
//    toOurMaterials.clear();
//    for (int i = 0; i < materialSize; ++i)
//    {
//        std::unique_ptr<Material> mat = std::make_unique<Material>();
//        mat->SetShader(Shader::GetDefault());
//        Utils::GLB::SetAssetName(mat.get(), jsonData, "materials", i);
//
//        nlohmann::json& matJson = jsonData["materials"][i];
//        // baseColorFactor
//        auto& pbrMetallicRoughness = matJson["pbrMetallicRoughness"];
//        std::array<float, 3> baseColorFactor =
//            pbrMetallicRoughness.value("baseColorFactor", std::array<float, 3>{1, 1, 1});
//        mat->SetVector("PBR", "baseColorFactor", {baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], 1});
//
//        // baseColorTex
//        if (matJson["pbrMetallicRoughness"].contains("baseColorTexture"))
//        {
//            int baseColorSamplerIndex = matJson["pbrMetallicRoughness"]["baseColorTexture"].value("index", -1);
//            if (baseColorSamplerIndex != -1)
//            {
//                int baseColorImageIndex = GetImageIndex(texJson[baseColorSamplerIndex]);
//                mat->SetTexture("baseColorTex", toOurTexture[baseColorImageIndex]);
//            }
//            mat->EnableFeature("_BaseColorMap");
//        }
//
//        // metallicRoughnessMap
//        if (matJson["pbrMetallicRoughness"].contains("metallicRoughnessTexture"))
//        {
//            int metallicRougnessSamplerIndex =
//                matJson["pbrMetallicRoughness"]["metallicRoughnessTexture"].value("index", -1);
//            if (metallicRougnessSamplerIndex != -1)
//            {
//                int metallicRoughnessImageIndex = GetImageIndex(texJson[metallicRougnessSamplerIndex]);
//                mat->SetTexture("metallicRoughnessMap", toOurTexture[metallicRoughnessImageIndex]);
//            }
//            mat->EnableFeature("_MetallicRoughnessMap");
//        }
//
//        // normal map
//        if (matJson.contains("normalTexture"))
//        {
//            int normalTextureSamplerIndex = matJson["normalTexture"].value("index", -1);
//            if (normalTextureSamplerIndex != -1)
//            {
//                int normalTextureImageInex = GetImageIndex(texJson[normalTextureSamplerIndex]);
//                mat->SetTexture("normalMap", toOurTexture[normalTextureImageInex]);
//            }
//            mat->EnableFeature("_NormalMap");
//        }
//
//        // emission
//        if (matJson.contains("emissiveTexture"))
//        {
//            int textureSamplerIndex = matJson["emissiveTexture"].value("index", -1);
//            if (textureSamplerIndex != -1)
//            {
//                int emissiveTextureImageIndex = GetImageIndex(texJson[textureSamplerIndex]);
//                mat->SetTexture("emissiveMap", toOurTexture[emissiveTextureImageIndex]);
//            }
//            mat->EnableFeature("_EmissiveMap");
//        }
//
//        auto shaderConfig = *mat->GetShaderConfig();
//        shaderConfig.cullMode = matJson.value("doubleSided", false) ? Gfx::CullMode::None : Gfx::CullMode::Back;
//        std::array<float, 3> emissive = {0, 0, 0};
//        std::string alphaModel = matJson.value("alphaMode", "OPAQUE");
//        shaderConfig.depth.testEnable = true;
//        if (shaderConfig.color.blends.empty())
//        {
//            shaderConfig.color.blends.push_back({});
//        }
//        if (alphaModel == "MASK")
//        {
//            mat->SetFloat("PBR", "alphaCutoff", matJson.value("alphaCutoff", 0.5f));
//            mat->EnableFeature("_AlphaTest");
//            auto& blend = shaderConfig.color.blends[0];
//            blend.blendEnable = false;
//        }
//        else if (alphaModel == "BLEND")
//        {
//            auto& blend = shaderConfig.color.blends[0];
//            blend.blendEnable = true;
//            blend.srcColorBlendFactor = Gfx::BlendFactor::Src_Alpha;
//            blend.dstColorBlendFactor = Gfx::BlendFactor::One_Minus_Src_Alpha;
//            blend.colorBlendOp = Gfx::BlendOp::Add;
//            blend.srcAlphaBlendFactor = Gfx::BlendFactor::Src_Alpha;
//            blend.dstAlphaBlendFactor = Gfx::BlendFactor::One_Minus_Src_Alpha;
//            blend.alphaBlendOp = Gfx::BlendOp::Add;
//        }
//        else
//        {
//            mat->SetFloat("PBR", "alphaCutoff", 0.0f);
//            auto& blend = shaderConfig.color.blends[0];
//            blend.blendEnable = false;
//        }
//        mat->SetShaderConfig(shaderConfig);
//
//        emissive = matJson.value("emissiveFactor", emissive);
//        mat->SetVector("PBR", "emissive", glm::vec4(emissive[0], emissive[1], emissive[2], 1.0f));
//        mat->SetFloat("PBR", "roughness", pbrMetallicRoughness.value("roughnessFactor", 1.0f));
//        mat->SetFloat("PBR", "metallic", pbrMetallicRoughness.value("metallicFactor", 1.0f));
//
//        toOurMaterials[i] = mat.get();
//        materials.push_back(std::move(mat));
//    }
//
//    if (jsonData.contains("meshes"))
//    {
//        for (auto& mesh : jsonData["meshes"])
//        {
//            for (auto& primitive : mesh["primitives"])
//            {
//                auto& attr = primitive["attributes"];
//                int matIndex = primitive.value("material", -1);
//                if (matIndex != -1)
//                {
//                    if (attr.contains("TANGENT"))
//                    {
//                        materials[matIndex]->EnableFeature("_Vertex_Tangent");
//                    }
//                    if (attr.contains("TEXCOORD_0"))
//                        materials[matIndex]->EnableFeature("_Vertex_UV0");
//                }
//            }
//        }
//    }
//
//    return true;
//}

std::vector<std::unique_ptr<GameObject>> Model::CreateGameObject(ModelNode& node, GameObject* parent)
{
    std::unique_ptr<GameObject> go = std::make_unique<GameObject>();

    glm::vec3 position;
    glm::vec3 scale;
    glm::quat rotation;
    Math::DecomposeMatrix(node.transform, position, scale, rotation);
    go->SetName(node.name);
    if (parent)
        go->SetParent(parent);
    go->SetLocalPosition(position);
    go->SetLocalScale(scale);
    go->SetLocalRotation(rotation);
    if (!node.meshes.empty())
    {
        std::vector<Material*> mats;
        std::vector<Mesh*> meshes;

        auto meshRenderer = go->AddComponent<MeshRenderer>();

        for (int i = 0; i < node.meshes.size(); ++i)
        {
            auto mat = this->materials[node.meshes[i].materialIndex].get();
            auto mesh = this->meshes[node.meshes[i].index].get();

            mats.push_back(mat);
            meshes.push_back(mesh);
        }

        meshRenderer->SetMaterials(mats);
        meshRenderer->SetMeshes(meshes);
    }

    std::vector<std::unique_ptr<GameObject>> gos;
    auto goTmp = go.get();
    gos.push_back(std::move(go));

    for (auto& n : node.children)
    {
        auto childGos = CreateGameObject(n, goTmp);
        gos.insert(gos.end(), std::make_move_iterator(childGos.begin()), std::make_move_iterator(childGos.end()));
    }

    return gos;
}

void Model::SetModel(
    ModelNode root,
    std::vector<std::unique_ptr<Mesh>>&& meshes,
    std::vector<std::unique_ptr<Texture>>&& textures,
    std::vector<std::unique_ptr<Material>>&& materials,
    std::vector<std::unique_ptr<Animation>>&& animations
)
{
    assimpLoaded = true;
    this->meshes = std::move(meshes);
    this->textures = std::move(textures);
    this->materials = std::move(materials);
    this->animations = std::move(animations);
    this->rootNode = root;

    SetMaterialKeywords(rootNode);
}

std::vector<std::unique_ptr<GameObject>> Model::CreateGameObject()
{
    if (assimpLoaded)
    {
        auto gos = CreateGameObject(rootNode, nullptr);
        if (!gos.empty())
        {
            for (auto& anim : animations)
            {
                auto animationPlayer = gos[0]->AddComponent<AnimationPlayer>();
                animationPlayer->SetAnimation(anim.get());
            }
        }
        return gos;
    }

    // create game objects that are presented in glb file
    nlohmann::json& scenesJson = jsonData["scenes"];
    std::vector<GameObject*> rootGameObjects;
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    for (int i = 0; i < scenesJson.size(); ++i)
    {
        nlohmann::json& sceneJson = scenesJson[i];
        std::unique_ptr<GameObject> rootGameObject = std::make_unique<GameObject>();
        Utils::GLB::SetAssetName(rootGameObject.get(), jsonData, "scenes", i);
        rootGameObject->SetName(std::string(sceneJson.value("name", "root")));

        for (int nodeIndex : sceneJson["nodes"])
        {
            auto gameObjectsCreated =
                CreateGameObjectFromNode(jsonData, nodeIndex, toOurMesh, rootGameObject.get(), GetDefaultMaterial());

            gameObjects.insert(
                gameObjects.end(),
                std::make_move_iterator(gameObjectsCreated.begin()),
                std::make_move_iterator(gameObjectsCreated.end())
            );
        }

        rootGameObjects.push_back(rootGameObject.get());
        gameObjects.push_back(std::move(rootGameObject));
    }

    std::unique_ptr<GameObject> root = std::make_unique<GameObject>();
    for (auto r : rootGameObjects)
    {
        r->SetParent(root.get());
    }

    gameObjects.insert(gameObjects.begin(), std::move(root));

    return gameObjects;
}

Material* Model::GetDefaultMaterial()
{
    return EngineInternalResources::GetDefaultMaterial();
}

std::vector<Asset*> Model::GetInternalAssets()
{
    std::vector<Asset*> assets(meshes.size() + textures.size() + materials.size() + animations.size());

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

    for (auto& obj : animations)
    {
        assets[i++] = obj.get();
    }

    return assets;
}

void Model::SetMaterialKeywords(ModelNode& node)
{
    if (!node.meshes.empty())
    {
        for (int i = 0; i < node.meshes.size(); ++i)
        {
            auto mat = this->materials[node.meshes[i].materialIndex].get();
            auto mesh = this->meshes[node.meshes[i].index].get();

            if (mesh->GetSubmeshes()[0].HasAttribute("tangent"))
            {
                mat->EnableFeature("_Vertex_Tangent");
            }
            if (mesh->GetSubmeshes()[0].HasAttribute("texCoords_0"))
            {
                mat->EnableFeature("_Vertex_UV0");
            }
        }
    }

    for (auto& n : node.children)
    {
        SetMaterialKeywords(n);
    }
}
