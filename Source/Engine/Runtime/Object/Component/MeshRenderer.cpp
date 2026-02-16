#include "MeshRenderer.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(Component, MeshRenderer, "00412ED6-89D3-4DD3-9D56-754820250E78");
MeshRenderer::MeshRenderer(GameObject* parent, Mesh* mesh, Material* material)
    : Component(parent), meshes(), materials({material})
{}

MeshRenderer::MeshRenderer(GameObject* parent) : MeshRenderer(parent, nullptr, nullptr) {}

MeshRenderer::MeshRenderer() : Component(nullptr), meshes(), materials() {};

TYPE_REFLECTION_MEMBER_VARIABLES(
    MeshRenderer,
    TYPE_REFLECTION_MEM(MeshRenderer, meshes),
    TYPE_REFLECTION_MEM(MeshRenderer, materials)
);

DEFINE_SERIALIZATION(
    MeshRenderer,
    Component,
    SER(meshes),
    SER(materials),
    SER(aabbMin, aabb.min),
    SER(aabbMax, aabb.max),
    SER(wantsToEnableSkinning),
    SER(isRayTracingEnabled)
);

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
    CheckSkeleton();
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

const std::string& MeshRenderer::GetName() const
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

    SetRayTracingEnabled(true);
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
    CheckSkeleton();

    if (!skinning.enabled && !meshes.empty() && !materials.empty())
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
                    spdlog::warn("bone not found {}, disabling skinning", bone.name);
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
            gpuResource->SetBuffer("skeleton", skinning.bonesBuffer.get());

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

void MeshRenderer::TransformChanged()
{
    aabbPositionNeedUpdate = true;
    if (auto scene = GetScene())
    {
        scene->GetRenderingScene().UpdateRenderer(*this);
    }
}

void MeshRenderer::CheckSkeleton()
{
    if (!meshes.empty() && meshes[0])
    {
        hasSkeleton = meshes[0]->HasSkeleton();
    }
    else
        hasSkeleton = false;
}

void MeshRenderer::OnLoaded()
{
    CheckSkeleton();
}

void MeshRenderer::SetRayTracingEnabled(bool enabled)
{
    isRayTracingEnabled = enabled;

    auto scene = GetScene();
    if (scene == nullptr)
        return;

    if (isRayTracingEnabled && !isRayTracingInitialized)
    {
        InitializeForRayTracing();
    }
}

void MeshRenderer::InitializeForRayTracing()
{
    auto scene = GetScene();
    if (scene == nullptr)
        return;

    auto& renderingScene = scene->GetRenderingScene();
    auto rayTracingContext = renderingScene.GetRayTracingContext();
    if (rayTracingContext == nullptr)
    {
        spdlog::warn("Ray tracing context is not available, cannot initialize ray tracing for MeshRenderer");
        return;
    }

    rayTracingMeshes.clear();
    rayTracingInstances.clear();

    auto worldMatrix = GetGameObject()->GetWorldMatrix();

    for (auto& mesh : meshes)
    {
        if (mesh == nullptr)
            continue;

        auto& submeshes = mesh->GetSubmeshes();
        std::vector<Gfx::BlasGeometry> geometries;
        geometries.reserve(submeshes.size());

        for (auto& submesh : mesh->GetSubmeshes())
        {
            geometries.push_back(Gfx::BlasGeometry{.vertexBuffer = submesh.GetVertexBuffer(), .vertexFormat = Gfx::GfxFormat::R32G32B32_SFloat, .vertexStride = sizeof(glm::vec3), .maxVertex = static_cast<uint32_t>(submesh.GetPositions().size()), .indexBuffer = submesh.GetIndexBuffer(), .indexBufferType = submesh.GetIndexBufferType(), .triangleCount = static_cast<uint32_t>(submesh.GetTriangleCount())});
        }

        if (!geometries.empty())
        {
            auto meshHandle = rayTracingContext->CreateBLAS(geometries);
            rayTracingMeshes.push_back(meshHandle);

            auto instanceHandle = rayTracingContext->CreateInstance(meshHandle, worldMatrix);
            rayTracingInstances.push_back(instanceHandle);
        }
    }

    isRayTracingInitialized = true;
}
