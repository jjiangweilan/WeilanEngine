#include "MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(MeshRenderer, "00412ED6-89D3-4DD3-9D56-754820250E78");
MeshRenderer::MeshRenderer(GameObject* parent, Mesh* mesh, Material* material)
    : Component(parent), meshes(), materials({material})
{}

MeshRenderer::MeshRenderer(GameObject* parent) : MeshRenderer(parent, nullptr, nullptr) {}

MeshRenderer::MeshRenderer() : Component(nullptr), meshes(), materials() {};

void MeshRenderer::SetMesh(Mesh* mesh)
{
    Mesh* meshes[] = {mesh};
    SetMeshes(meshes);
}

void MeshRenderer::SetMaterial(Material* material)
{
    Material* mats[] = {material};
    SetMaterials(mats);
}

void MeshRenderer::SetMeshes(std::span<Mesh*> meshes)
{
    this->meshes.clear();
    for (auto m : meshes)
        this->meshes.push_back(m);
    this->materials.resize(meshes.size());
    aabbBoundsNeedUpdate = true;
}

void MeshRenderer::UpdateAABB()
{
    glm::vec3 min =
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    glm::vec3 max = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    bool isValid = false;
    for (auto mesh : meshes)
    {
        if (mesh != nullptr)
        {
            isValid = true;
            auto& aabb = mesh->GetAABB();
            min = glm::min(min, aabb.min);
            max = glm::max(max, aabb.max);
        }
    }

    if (isValid)
        aabb = {min, max};
}
void MeshRenderer::SetMaterials(std::span<ObjPtr<Material>> materials)
{
    this->materials = std::vector<ObjPtr<Material>>(materials.begin(), materials.end());
}
void MeshRenderer::SetMaterials(std::span<Material*> materials)
{
    this->materials.clear();
    for (auto m : materials)
        this->materials.push_back(m);
}

Mesh* MeshRenderer::GetMesh()
{
    ValidateSkinning();
    return meshes.empty() ? nullptr : meshes[0];
}

std::span<ObjPtr<Mesh>> MeshRenderer::GetMeshes()
{
    ValidateSkinning();
    return meshes;
}

const std::vector<ObjPtr<Material>>& MeshRenderer::GetMaterials()
{
    return materials;
}

void MeshRenderer::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("meshes", meshes);
    s->Serialize("materials", materials);
    s->Serialize("aabbMin", aabb.min);
    s->Serialize("aabbMax", aabb.max);
    s->Serialize("wantsToEnableSkinning", wantsToEnableSkinning);
}

void MeshRenderer::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("meshes", meshes, [this](void* res) { aabbBoundsNeedUpdate = true; });
    s->Deserialize("materials", materials);
    s->Deserialize("aabbMin", aabb.min);
    s->Deserialize("aabbMax", aabb.max);
    s->Deserialize("wantsToEnableSkinning", wantsToEnableSkinning);
}

std::unique_ptr<Component> MeshRenderer::Clone(GameObject& owner)
{
    auto clone = std::make_unique<MeshRenderer>(&owner);

    clone->meshes = meshes;
    clone->materials = materials;

    if (IsEnabled())
    {
        clone->enabled = true;
    }

    return clone;
}

const std::string& MeshRenderer::GetName()
{
    static std::string name = "MeshRenderer";
    return name;
}

void MeshRenderer::AddToRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->AddRenderer(*this);
    }
}
void MeshRenderer::RemoveFromRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->RemoveRenderer(*this);
    }
}

void MeshRenderer::OnEnable()
{
    AddToRenderingScene();
}
void MeshRenderer::OnDisable()
{
    RemoveFromRenderingScene();
}

AABB MeshRenderer::GetAABB()
{
    if (aabbBoundsNeedUpdate)
    {
        UpdateAABB();
        aabbBoundsNeedUpdate = false;
        aabbPositionNeedUpdate = true;
    }

    if (aabbPositionNeedUpdate)
    {
        auto model = GetGameObject()->GetWorldMatrix();
        aabbWS = this->aabb;
        aabbWS.Transform(glm::mat3(model), model[3]);
        aabbPositionNeedUpdate = false;
    }

    return aabbWS;
}

void MeshRenderer::Tick() {}

void MeshRenderer::UpdateSkinning()
{
    if (skinning.enabled)
    {

        Skinning::GPUBoneTransforms boneTransforms;
        int maxBoneCount = skinning.bones.size();
        for (int bi = 0; bi < maxBoneCount && bi < Skinning::MaxBoneSize; bi++)
        {
            boneTransforms.boneTrnasforms[bi] = skinning.bones[bi]->GetWorldMatrix() * skinning.tposeMatrix[bi];
        }

        GetGfxDriver()
            ->UploadBuffer(*skinning.bonesBuffer, (uint8_t*)&boneTransforms, sizeof(Skinning::GPUBoneTransforms));
    }
}

void MeshRenderer::ValidateSkinning()
{
    if (!meshes.empty() && !materials.empty() && !skinning.enabled)
    {
        auto mesh = meshes[0];
        if (mesh != nullptr && mesh->HasSkeleton())
        {
            auto go = GetGameObject();
            auto skeleton = mesh->GetSkeleton();

            if (skeleton.size() > Skinning::MaxBoneSize)
            {
                spdlog::error("Exceeding maximum bone size");
                return;
            }

            skinning.bones.clear();
            skinning.tposeMatrix.clear();
            for (auto& bone : skeleton)
            {
                GameObject* boneGO = nullptr;
                if (auto parent = go->GetParent())
                    boneGO = parent->Find(bone.name);
                else
                    boneGO = go->Find(bone.name);

                if (boneGO == nullptr)
                {
                    spdlog::warn("bone not found {}, skining is not enabled", bone.name);
                    DisableSkinning();
                    return;
                }
                skinning.bones.push_back(boneGO);
                skinning.tposeMatrix.push_back(bone.offsetMatrix);
            }

            // cpu is ready, let's prepare gpu resources
            skinning.bonesBuffer = GetGfxDriver()->CreateBuffer(
                sizeof(Skinning::GPUBoneTransforms),
                Gfx::BufferUsage::Uniform,
                false,
                false,
                "skinning buffer"
            );
            skinning.enabled = true;
            wantsToEnableSkinning = true;

            gpuResource = GetGfxDriver()->CreateShaderResource();
            gpuResource->SetBuffer("BoneTransform", skinning.bonesBuffer.get());

            Skinning::GPUBoneTransforms boneTransforms;
            int maxBoneCount = skinning.bones.size();
            for (int bi = 0; bi < maxBoneCount && bi < Skinning::MaxBoneSize; bi++)
            {
                boneTransforms.boneTrnasforms[bi] = glm::mat4(1.0f);
            }

            GetGfxDriver()
                ->UploadBuffer(*skinning.bonesBuffer, (uint8_t*)&boneTransforms, sizeof(Skinning::GPUBoneTransforms));
            return;
        }
    }
}
void MeshRenderer::DisableSkinning()
{
    // clear all gpu resources
    if (skinning.enabled)
    {
        wantsToEnableSkinning = false;
        skinning.enabled = false;
        skinning.bonesBuffer = nullptr;
        skinning.bones.clear();
        skinning.tposeMatrix.clear();
        gpuResource = nullptr; // currently only used for skinning, so let's destroy this too
    }
}
bool MeshRenderer::IsSkinningEnabled()
{
    return skinning.enabled;
}

void MeshRenderer::OnDrawGizmos() {}
