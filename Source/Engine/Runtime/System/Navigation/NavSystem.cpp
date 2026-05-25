#include "./NavSystem.hpp"

#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"

namespace
{
NavSystem* globalNavSystem = nullptr;
}

void NavSystem::Init(ObjPtr<NavData> data)
{
    navData = data;
    if (navData)
    {
        runtimeCells.clear();
        runtimeCells.reserve(navData->grid.cells.size());
        for (auto& cell : navData->grid.cells)
        {
            runtimeCells.push_back({cell, false});
        }
    }
}

void NavSystem::Visualize() const
{
    NavData* data = navData.Get();
    if (data == nullptr)
    {
        return;
    }

    const NavGrid& grid = data->grid;
    const NavDataConfig& config = grid.config;
    if (config.resolution.x <= 0.0f || config.resolution.y <= 0.0f || config.width <= 0 || config.height <= 0)
    {
        return;
    }

    const int expectedCellCount = config.width * config.height;
    if (grid.cells.size() < expectedCellCount)
    {
        return;
    }

    auto cellIndex = [width = config.width](int x, int y)
    { return y * width + x; };
    auto cellPosition = [&](int x, int y)
    {
        const NavCell& cell = grid.cells[cellIndex(x, y)];
        return relativePosition + config.origin + float3((static_cast<float>(x) + 0.5f) * config.resolution.x, cell.height, (static_cast<float>(y) + 0.5f) * config.resolution.y);
    };

    constexpr float4 gridColor(0.1f, 1.0f, 0.2f, 1.0f);
    constexpr float4 occupiedColor(1.0f, 0.0f, 0.0f, 1.0f);
    std::vector<Graphics::Line> lines;
    lines.reserve((config.width - 1) * config.height + config.width * (config.height - 1) + runtimeCells.size() * 4);
    for (int y = 0; y < config.height; ++y)
    {
        for (int x = 0; x < config.width; ++x)
        {
            const int index = cellIndex(x, y);
            const float3 center = cellPosition(x, y);
            if (x + 1 < config.width)
            {
                lines.push_back({center, cellPosition(x + 1, y), gridColor});
            }
            if (y + 1 < config.height)
            {
                lines.push_back({center, cellPosition(x, y + 1), gridColor});
            }

            if (runtimeCells.size() > index && runtimeCells[index].occupied)
            {
                const float halfWidth = config.resolution.x * 0.5f;
                const float halfHeight = config.resolution.y * 0.5f;
                const float3 p0 = center + float3(-halfWidth, 0.0f, -halfHeight);
                const float3 p1 = center + float3(halfWidth, 0.0f, -halfHeight);
                const float3 p2 = center + float3(halfWidth, 0.0f, halfHeight);
                const float3 p3 = center + float3(-halfWidth, 0.0f, halfHeight);
                lines.push_back({p0, p1, occupiedColor});
                lines.push_back({p1, p2, occupiedColor});
                lines.push_back({p2, p3, occupiedColor});
                lines.push_back({p3, p0, occupiedColor});
            }
        }
    }
    Graphics::DrawLines(lines);
}

NavSystem* NavSystem::GetGlobalInstance()
{
    return globalNavSystem;
}

void NavSystem::SetGlobalInstance(NavSystem* system)
{
    globalNavSystem = system;
}

void NavSystem::ClearGlobalInstance(NavSystem* system)
{
    if (globalNavSystem == system)
    {
        globalNavSystem = nullptr;
    }
}

uint64_t NavObjectHandle::GenerateHandleID()
{
    static uint64_t id = 0;
    return id++;
}

NavObjectHandle NavSystem::AddNavObject(GameObject* gameObject)
{
    NavObjectHandle handle;
    handle.handleId = handle.GenerateHandleID();

    std::vector<MeshRenderer*> renderers = gameObject->GetComponentsInChildren<MeshRenderer>();
    registeredNavObjects[handle.handleId] = {gameObject, std::move(renderers)};

    RebuildRuntimeOccupancy();

    return handle;
}

void NavSystem::UpdateRuntimeNavObject(NavObjectHandle handle)
{
    auto it = registeredNavObjects.find(handle.handleId);
    if (it == registeredNavObjects.end())
        return;

    it->second.meshRenderers = it->second.go->GetComponentsInChildren<MeshRenderer>();

    RebuildRuntimeOccupancy();
}

void NavSystem::RemoveNavObject(NavObjectHandle handle)
{
    registeredNavObjects.erase(handle.handleId);
    RebuildRuntimeOccupancy();
}

void NavSystem::RebuildRuntimeOccupancy()
{
    NavData* data = navData.Get();
    if (data == nullptr)
        return;

    const NavGrid& grid = data->grid;
    const NavDataConfig& config = grid.config;
    if (config.resolution.x <= 0.0f || config.resolution.y <= 0.0f || config.width <= 0 || config.height <= 0)
        return;

    const int width = config.width;
    const int height = config.height;
    const int expectedCellCount = width * height;
    if (grid.cells.size() < expectedCellCount)
        return;

    const float3 origin = config.origin;
    const float2 resolution = config.resolution;

    if (runtimeCells.size() != expectedCellCount)
    {
        runtimeCells.clear();
        runtimeCells.reserve(expectedCellCount);
        for (auto& cell : grid.cells)
        {
            runtimeCells.push_back({cell, false});
        }
    }
    else
    {
        for (int i = 0; i < expectedCellCount; ++i)
        {
            runtimeCells[i].cell = grid.cells[i];
            runtimeCells[i].occupied = false;
        }
    }

    const float sphereRadius = std::min(resolution.x, resolution.y) * 0.5f;
    const float3 castDir(0.0f, 1.0f, 0.0f);

    for (auto& [handleId, registered] : registeredNavObjects)
    {
        for (MeshRenderer* renderer : registered.meshRenderers)
        {
            if (renderer == nullptr)
                continue;

            std::span<ObjPtr<Mesh>> meshes = renderer->GetMeshes();
            glm::mat4 worldMatrix = renderer->GetGameObject()->GetWorldMatrix();

            for (ObjPtr<Mesh>& meshPtr : meshes)
            {
                Mesh* mesh = meshPtr.Get();
                if (mesh == nullptr)
                    continue;

                const auto& submeshes = mesh->GetSubmeshes();
                for (const Submesh& submesh : submeshes)
                {
                    for (int y = 0; y < height; ++y)
                    {
                        for (int x = 0; x < width; ++x)
                        {
                            int index = y * width + x;
                            if (runtimeCells[index].occupied)
                                continue;

                            const float3 cellCenter(
                                relativePosition.x + origin.x + (static_cast<float>(x) + 0.5f) * resolution.x,
                                relativePosition.y + runtimeCells[index].cell.height,
                                relativePosition.z + origin.z + (static_cast<float>(y) + 0.5f) * resolution.y
                            );

                            Sphere sphere{cellCenter, sphereRadius};
                            float distance = 0.0f;
                            if (SphereVsMesh(sphere, castDir, &submesh, worldMatrix, distance))
                            {
                                runtimeCells[index].occupied = true;
                            }
                        }
                    }
                }
            }
        }
    }
}
