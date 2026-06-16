#include "ModelLoader.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Runtime/System/AssetDatabase/ArtifactTypes.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/AssetDatabase/ModelArtifact.hpp"
#include <fstream>

DEFINE_ASSET_LOADER(ModelLoader, "glb,gltf,fbx")

const std::vector<std::type_index>& ModelLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Model)};
    return types;
}

void ModelLoader::Load()
{
    UUID sourceAssetUUID = assetUUID;

    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Texture>> textures;
    std::vector<std::unique_ptr<Material>> materials;
    std::vector<std::unique_ptr<AnimationClip>> animationClips;
    std::vector<std::unique_ptr<AnimationSet>> animationSets;
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<ObjPtr<GameObject>> roots;

    for (const auto& record : importDatabase->ListArtifacts(sourceAssetUUID, AssetArtifacts::Kind::Texture))
    {
        auto data = importDatabase->ReadArtifactFile(record.relativePath);
        if (data.size() == 0)
        {
            continue;
        }

        auto texture = std::make_unique<Texture>(KtxTexture{data.data(), data.size()}, record.artifactUUID);
        texture->SetName(record.name);
        textures.push_back(std::move(texture));
    }

    for (const auto& record : importDatabase->ListArtifacts(sourceAssetUUID, AssetArtifacts::Kind::Mesh))
    {
        auto mesh = ModelArtifact::ReadMeshBlob(importDatabase->ReadArtifactFile(record.relativePath));
        if (mesh == nullptr)
        {
            continue;
        }

        mesh->SetUUID(record.artifactUUID);
        if (!record.name.empty())
        {
            mesh->SetName(record.name);
        }
        meshes.push_back(std::move(mesh));
    }

    for (const auto& record : importDatabase->ListArtifacts(sourceAssetUUID, AssetArtifacts::Kind::AnimationClip))
    {
        auto clip = ModelArtifact::ReadAnimationClipBlob(importDatabase->ReadArtifactFile(record.relativePath));
        if (clip == nullptr)
        {
            continue;
        }

        clip->SetUUID(record.artifactUUID);
        if (!record.name.empty())
        {
            clip->SetName(record.name);
        }
        animationClips.push_back(std::move(clip));
    }

    for (const auto& record : importDatabase->ListArtifacts(sourceAssetUUID, AssetArtifacts::Kind::AnimationSet))
    {
        auto animationSet = ModelArtifact::ReadAnimationSetBlob(importDatabase->ReadArtifactFile(record.relativePath));
        if (animationSet == nullptr)
        {
            continue;
        }

        animationSet->SetUUID(record.artifactUUID);
        if (!record.name.empty())
        {
            animationSet->SetName(record.name);
        }
        animationSets.push_back(std::move(animationSet));
    }

    for (const auto& record : importDatabase->ListArtifacts(sourceAssetUUID, AssetArtifacts::Kind::Material))
    {
        auto materialData = importDatabase->ReadArtifactFile(record.relativePath);
        if (materialData.size() == 0)
        {
            continue;
        }

        SerializeReferenceResolveMap resolveMap;
        std::vector<uint8_t> binary(materialData.data(), materialData.data() + materialData.size());
        JsonSerializer serializer(binary, &resolveMap);
        auto material = std::make_unique<Material>();
        material->Deserialize(&serializer);
        material->SetUUID(record.artifactUUID);
        if (!record.name.empty())
        {
            material->SetName(record.name);
        }

        for (const UUID& referencedUUID : serializer.GetReferencedObjects())
        {
            if (!referencedUUID.IsEmpty())
            {
                AssetDatabase::Singleton()->LoadAssetByID(referencedUUID);
            }
        }

        materials.push_back(std::move(material));
    }

    std::filesystem::path modelArtifactPath;
    if (!importDatabase->TryGetArtifactPath(sourceAssetUUID.ToString(), AssetArtifacts::Kind::Model, modelArtifactPath))
    {
        return;
    }

    if (!ModelArtifact::ReadModelGraph(importDatabase->ReadArtifactFile(modelArtifactPath), gameObjects, roots))
    {
        return;
    }

    auto model = std::make_unique<Model>();
    model->SetModelGraph(
        std::move(gameObjects),
        std::move(roots),
        std::move(meshes),
        std::move(textures),
        std::move(materials),
        std::move(animationClips),
        std::move(animationSets)
    );

    asset = std::move(model);
}
