#include "Model.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <fstream>

DEFINE_ASSET(Model, "F675BB06-829E-43B4-BF53-F9518C7A94DB", "glb,fbx");

static std::size_t WriteAccessorDataToBuffer(
    nlohmann::json& j, unsigned char* dstBuffer, std::size_t dstOffset, unsigned char* srcBuffer, int accessorIndex
);

static void MoveOwningChildrenToFlatList(std::unique_ptr<GameObject>& root, std::vector<std::unique_ptr<GameObject>>& out)
{
    auto owningChildren = root->GetOwningChildren();
    out.push_back(std::move(root));
    for (auto& child : owningChildren)
    {
        MoveOwningChildrenToFlatList(child, out);
    }
}

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
        auto physicsBody = go->AddComponent<PhysicsBody>();

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

    std::vector<std::unique_ptr<GameObject>> gos{};
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
    std::vector<std::unique_ptr<AnimationClip>>&& animationClips,
    std::vector<std::unique_ptr<AnimationSet>>&& animationSets
)
{
    assimpLoaded = true;
    this->meshes = std::move(meshes);
    this->textures = std::move(textures);
    this->materials = std::move(materials);
    this->animationClips = std::move(animationClips);
    this->animationSets = std::move(animationSets);
    this->rootNode = root;

    SetMaterialKeywords(rootNode);
}

void Model::SetModelGraph(
    std::vector<std::unique_ptr<GameObject>>&& gameObjects,
    std::vector<ObjPtr<GameObject>>&& roots,
    std::vector<std::unique_ptr<Mesh>>&& meshes,
    std::vector<std::unique_ptr<Texture>>&& textures,
    std::vector<std::unique_ptr<Material>>&& materials,
    std::vector<std::unique_ptr<AnimationClip>>&& animationClips,
    std::vector<std::unique_ptr<AnimationSet>>&& animationSets
)
{
    assimpLoaded = false;
    this->modelGameObjects = std::move(gameObjects);
    this->modelRoots = std::move(roots);
    this->meshes = std::move(meshes);
    this->textures = std::move(textures);
    this->materials = std::move(materials);
    this->animationClips = std::move(animationClips);
    this->animationSets = std::move(animationSets);
}

std::vector<std::unique_ptr<GameObject>> Model::CreateGameObject()
{
    if (!modelRoots.empty())
    {
        std::vector<std::unique_ptr<GameObject>> created;
        for (auto root : modelRoots)
        {
            if (root == nullptr)
            {
                continue;
            }

            auto rootCopy = std::make_unique<GameObject>(*root);
            MoveOwningChildrenToFlatList(rootCopy, created);
        }
        return created;
    }

    if (assimpLoaded)
    {
        auto gos = CreateGameObject(rootNode, nullptr);
        if (!gos.empty())
        {
            for (auto& animationSet : animationSets)
            {
                auto animationPlayer = gos[0]->AddComponent<AnimationPlayer>();
                animationPlayer->SetAnimationSet(animationSet.get());
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
    std::vector<Asset*> assets(meshes.size() + textures.size() + materials.size() + animationClips.size() + animationSets.size());

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

    for (auto& obj : animationClips)
    {
        assets[i++] = obj.get();
    }

    for (auto& obj : animationSets)
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

void Model::OnLoaded()
{
    std::for_each(materials.begin(), materials.end(), [](auto& m) { m->OnLoaded(); });
    for (auto& go : modelGameObjects)
    {
        if (go)
        {
            go->OnLoaded();
        }
    }
}
