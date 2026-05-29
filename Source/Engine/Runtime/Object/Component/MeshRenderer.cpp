#include "MeshRenderer.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
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
    // SER(isRayTracingEnabled),
    SER(isGPUObject)
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

void MeshRenderer::SetMaterialSize(int size)
{
    if (size >= 0)
    {
        materials.resize(size);
        RefreshGPUSceneObjects();
    }
}

size_t MeshRenderer::GetSubmeshDrawSlotCount() const
{
    size_t count = 0;
    for (const auto& mesh : meshes)
    {
        if (mesh != nullptr)
            count += mesh->GetSubmeshes().size();
    }

    return count;
}

void MeshRenderer::SetMeshes(std::span<Mesh*> meshes)
{
    auto oldMaterials = std::move(this->materials);
    this->meshes.clear();
    for (auto m : meshes)
        this->meshes.push_back(m);
    this->materials = std::move(oldMaterials);
    if (!multipass)
        this->materials.resize(GetSubmeshDrawSlotCount());
    aabbBoundsNeedUpdate = true;
    CheckSkeleton();
    RefreshGPUSceneObjects();
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
    RefreshGPUSceneObjects();
}
void MeshRenderer::SetMaterials(std::span<Material*> materials)
{
    this->materials.clear();
    for (auto m : materials)
        this->materials.push_back(m);
    RefreshGPUSceneObjects();
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

void MeshRenderer::AddToBVHScene()
{
    if (Scene* scene = GetScene())
    {
        bvhHandle = scene->GetBVHScene().AddNode(BVHNode{
            .type = BVHNodeType::MeshRenderer,
            .isValid = [this]()
            {
                auto go = GetGameObject();
                return GetScene() != nullptr && go != nullptr && IsEnabled() && go->IsActiveInScene(); },
            .getAABB = [this]()
            { return GetAABB(); },
            .owner = this,
        });
    }
}
void MeshRenderer::RemoveFromBVHScene()
{
    if (Scene* scene = GetScene())
    {
        scene->GetBVHScene().RemoveNode(bvhHandle);
        bvhHandle = {};
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
    AddToBVHScene();

    if (isGPUObject)
        RegisterGPUSceneObjects();
    else
        AddToRenderingScene();

    InitializeForRayTracing();
}

void MeshRenderer::OnDisable()
{
    RemoveFromBVHScene();

    if (isGPUObject)
        UnregisterGPUSceneObjects();
    else
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

void MeshRenderer::OnStart()
{
}

void MeshRenderer::Tick() {}

void MeshRenderer::UpdateSkinning()
{
    if (skinning.enabled)
    {
        Skinning::GPUBoneTransforms boneTransforms = BuildSkinningBoneTransforms();

        GetGfxDriver()
            ->UploadBuffer(*skinning.bonesBuffer, (uint8_t*)&boneTransforms, sizeof(Skinning::GPUBoneTransforms));

        UpdateGPUDrivenSkinningData();
    }
}

MeshRenderer::Skinning::GPUBoneTransforms MeshRenderer::BuildSkinningBoneTransforms() const
{
    Skinning::GPUBoneTransforms boneTransforms{};
    int maxBoneCount = skinning.bones.size();
    for (int bi = 0; bi < maxBoneCount && bi < Skinning::MaxBoneSize; bi++)
    {
        boneTransforms.boneTrnasforms[bi] = skinning.bones[bi]->GetWorldMatrix() * skinning.tposeMatrix[bi];
    }

    return boneTransforms;
}

void MeshRenderer::UpdateGPUDrivenSkinningData()
{
    if (!gpuObjectRegistered || !skinning.enabled || !gpuSkinningDescriptor.dataAlloc.IsValid())
        return;

    Skinning::GPUBoneTransforms boneTransforms = BuildSkinningBoneTransforms();
    Rendering::GPUDrivenManager::Instance().UpdateSkinningData(
        gpuSkinningDescriptor,
        &boneTransforms,
        sizeof(Skinning::GPUBoneTransforms)
    );
}

void MeshRenderer::FreeGPUDrivenSkinningData()
{
    if (!gpuSkinningDescriptor.dataAlloc.IsValid())
        return;

    auto* gpuDriven = Rendering::GPUDrivenManager::TryGetInstance();
    if (gpuDriven != nullptr)
        gpuDriven->FreeSkinningData(gpuSkinningDescriptor);
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
        FreeGPUDrivenSkinningData();
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

    if (isGPUObject && gpuObjectRegistered)
        UpdateGPUSceneObjectTransforms();
    else if (auto scene = GetScene())
    {
        scene->GetBVHScene().MarkDirty(bvhHandle);
    }

    if (isRayTracingInitialized && rayTracingInstance >= 0)
    {
        if (auto scene = GetScene())
        {
            scene->GetRenderingScene().UpdateRayTracingInstance(rayTracingInstance, GetGameObject()->GetWorldMatrix());
        }
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

void MeshRenderer::EnableRayTracing(bool enable)
{
    isRayTracingEnabled = enable;

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

    rayTracingMesh = -1;
    rayTracingInstance = -1;

    auto worldMatrix = GetGameObject()->GetWorldMatrix();
    std::vector<Gfx::BlasGeometry> geometries{};

    uint64_t globalBufferShaderDeviceAddress = Rendering::GPUDrivenManager::Instance().GetGlobalBufferShaderDeviceAddress();
    for (auto& mesh : meshes)
    {
        if (mesh == nullptr)
            continue;

        for (auto& submesh : mesh->GetSubmeshes())
        {
            geometries.push_back(Gfx::BlasGeometry{
                .vertexBufferShaderDeviceAddress = submesh.GetVertexBufferShaderDeviceAddress(),
                .vertexFormat = Gfx::GfxFormat::R32G32B32_SFloat,
                .vertexStride = sizeof(glm::vec3),
                .maxVertex = static_cast<uint32_t>(submesh.GetPositions().size()),
                .indexBufferShaderDeviceAddress = submesh.GetIndexBufferShaderDeviceAddress(),
                .indexBufferType = submesh.GetIndexBufferType(),
                .triangleCount = static_cast<uint32_t>(submesh.GetIndexCount() / 3),
            });
        }
    }

    if (!geometries.empty())
    {
        auto meshHandle = renderingScene.CreateBLAS(geometries);
        rayTracingMesh = meshHandle;

        auto instanceHandle = renderingScene.CreateInstance(this, meshHandle, worldMatrix);
        rayTracingInstance = instanceHandle;

        isRayTracingInitialized = true;
    }
}

// --- GPU-Driven ---

void MeshRenderer::SetGPUObject(bool enabled)
{
    if (isGPUObject == enabled)
        return;

    isGPUObject = enabled;

    if (IsEnabled())
    {
        if (isGPUObject)
        {
            RemoveFromRenderingScene();
            RegisterGPUSceneObjects();
        }
        else
        {
            AddToRenderingScene();
            UnregisterGPUSceneObjects();
        }
    }
}

void MeshRenderer::RegisterGPUSceneObjects()
{
    ValidateSkinning();

    if (gpuObjectRegistered || meshes.empty() || materials.empty())
        return;

    gpuGeometries.clear();
    gpuRenderMaterials.clear();
    auto& gpuDriven = Rendering::GPUDrivenManager::Instance();

    if (meshes.empty())
        return;

    auto worldMatrix = GetGameObject()->GetWorldMatrix();
    auto invTspBase = glm::mat4(glm::inverse(glm::transpose(glm::mat3(worldMatrix))));

    int drawSlotIndex = 0;
    std::vector<Rendering::GpuRenderData> renderDatas;

    for (int i = 0; i < static_cast<int>(meshes.size()); ++i)
    {
        auto mesh = meshes[i].Get();
        if (mesh == nullptr)
            continue;

        for (auto& submesh : mesh->GetSubmeshes())
        {
            auto material = drawSlotIndex < static_cast<int>(materials.size()) ? materials[drawSlotIndex].Get() : nullptr;
            drawSlotIndex++;

            if (material == nullptr)
                continue;

            // Ensure material is registered for GPU-driven
            if (!material->IsGPUMaterialRegistered())
                material->RegisterGPUMaterial();

            Rendering::GpuRenderData renderData;
            renderData.geometryOffset = submesh.GetGpuGeometryOffset();
            renderData.materialOffset = material->GetGpuMaterialOffset();
            renderData.shaderID = material->GetShaderProgram()->GetShaderID();

            renderDatas.push_back(renderData);
            gpuGeometries.push_back(submesh.GetGpuGeometry());
            gpuRenderMaterials.push_back(material);
        }
    }

    if (renderDatas.empty())
        return;

    renderDataListHandle = gpuDriven.RegisterRenderDataList(renderDatas);

    uint32_t skeletonOffset = Rendering::InvalidTextureIndex;
    if (skinning.enabled)
    {
        gpuSkinningDescriptor = gpuDriven.AllocateSkinningData(sizeof(Skinning::GPUBoneTransforms));
        if (gpuSkinningDescriptor.dataAlloc.IsValid())
        {
            Skinning::GPUBoneTransforms boneTransforms = BuildSkinningBoneTransforms();
            gpuDriven.UpdateSkinningData(gpuSkinningDescriptor, &boneTransforms, sizeof(Skinning::GPUBoneTransforms));
            skeletonOffset = static_cast<uint32_t>(gpuSkinningDescriptor.dataAlloc.offset);
        }
    }

    gpuObjectHandle = gpuDriven.RegisterObject(worldMatrix, invTspBase, renderDataListHandle, skeletonOffset);
    gpuObjectDescriptor = gpuDriven.GetObjectDescriptor(gpuObjectHandle);
    gpuRenderDataListDescriptor = gpuDriven.GetRenderDataListDescriptor(renderDataListHandle);
    gpuObjectRegistered = true;

    // Register to GPU object list in RenderingScene
    if (auto scene = GetScene())
    {
        scene->GetRenderingScene().AddGPUObjectRenderer(*this);
    }
}

void MeshRenderer::UnregisterGPUSceneObjects()
{
    if (!gpuObjectRegistered)
        return;

    auto& gpuDriven = Rendering::GPUDrivenManager::Instance();
    gpuDriven.UnregisterObject(gpuObjectHandle);
    gpuDriven.UnregisterRenderDataList(renderDataListHandle);
    FreeGPUDrivenSkinningData();
    gpuGeometries.clear();
    gpuRenderMaterials.clear();
    gpuObjectRegistered = false;

    if (auto scene = GetScene())
    {
        scene->GetRenderingScene().RemoveGPUObjectRenderer(*this);
    }
}

void MeshRenderer::UpdateGPUSceneObjectTransforms()
{
    if (!gpuObjectRegistered)
        return;

    auto& gpuDriven = Rendering::GPUDrivenManager::Instance();

    const auto& renderDataListDescriptor = gpuDriven.GetRenderDataListDescriptor(renderDataListHandle);
    Rendering::GpuObject gpuObject{
        .model = GetGameObject()->GetWorldMatrix(),
        .invTspModel = glm::mat4(glm::inverse(glm::transpose(glm::mat3(GetGameObject()->GetWorldMatrix())))),
        .renderDataCount = (uint32_t)renderDataListDescriptor.renderDataList.size(),
        .pRenderDataOffset = (uint32_t)renderDataListDescriptor.dataAlloc.offset,
        .skeletonOffset = gpuSkinningDescriptor.dataAlloc.IsValid() ? static_cast<uint32_t>(gpuSkinningDescriptor.dataAlloc.offset) : Rendering::InvalidTextureIndex,
    };

    gpuDriven.UpdateObject(gpuObjectHandle, gpuObject);
}

void MeshRenderer::RefreshGPUSceneObjects()
{
    if (!isGPUObject || !IsEnabled() || GetScene() == nullptr)
        return;

    if (gpuObjectRegistered)
        UnregisterGPUSceneObjects();

    RegisterGPUSceneObjects();
}
