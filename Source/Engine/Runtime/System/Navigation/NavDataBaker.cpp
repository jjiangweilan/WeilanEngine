#include "NavDataBaker.hpp"

#include "Engine/Core/JobSystem.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"

#include <glm/gtx/intersect.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
struct BakeMesh
{
    Mesh* mesh = nullptr;
    float4x4 worldMatrix = float4x4(1.0f);
};

void ExpandWorldAABB(const AABB& localAabb, const float4x4& worldMatrix, float3& outMin, float3& outMax)
{
    const float3 min = localAabb.min;
    const float3 max = localAabb.max;
    const float3 corners[] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, max.y, min.z},
        {min.x, max.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
    };

    for (const float3& corner : corners)
    {
        float3 worldCorner = float3(worldMatrix * float4(corner, 1.0f));
        outMin = glm::min(outMin, worldCorner);
        outMax = glm::max(outMax, worldCorner);
    }
}
} // namespace

void NavDataBaker::Bake(std::span<MeshRenderer*> renderers, NavData& navData)
{
    std::vector<BakeMesh> bakeMeshes;
    bakeMeshes.reserve(renderers.size());

    for (MeshRenderer* renderer : renderers)
    {
        if (renderer == nullptr || renderer->GetGameObject() == nullptr)
            continue;

        const float4x4 worldMatrix = renderer->GetGameObject()->GetWorldMatrix();
        std::span<ObjPtr<Mesh>> meshes = renderer->GetMeshes();
        for (const ObjPtr<Mesh>& mesh : meshes)
        {
            if (mesh != nullptr)
                bakeMeshes.push_back({mesh.Get(), worldMatrix});
        }
    }

    if (bakeMeshes.empty())
    {
        navData.ClearCells();
        return;
    }

    NavDataConfig& config = navData.grid.config;

    float3 worldMin(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    );
    float3 worldMax(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    );

    for (const BakeMesh& bakeMesh : bakeMeshes)
        ExpandWorldAABB(bakeMesh.mesh->GetAABB(), bakeMesh.worldMatrix, worldMin, worldMax);

    const float extentX = worldMax.x - worldMin.x;
    const float extentZ = worldMax.z - worldMin.z;

    if (config.resolution.x <= 0 || config.resolution.y <= 0 || extentX <= 0 || extentZ <= 0)
    {
        navData.ClearCells();
        return;
    }

    config.width = static_cast<int>(std::ceil(extentX / config.resolution.x));
    config.height = static_cast<int>(std::ceil(extentZ / config.resolution.y));
    config.origin = float3(worldMin.x, 0.0f, worldMin.z);

    const int width = config.width;
    const int height = config.height;
    const int cellCount = width * height;
    navData.grid.cells.clear();
    navData.grid.cells.resize(cellCount);

    auto cellIndex = [width](int x, int y) { return y * width + x; };
    const float rayOriginY = worldMax.y + 1.0f;
    const float3 rayDirection(0.0f, -1.0f, 0.0f);

    const float originX = config.origin.x;
    const float originZ = config.origin.z;
    const float maxWalkableSlopeRadians = std::clamp(config.maxWalkableSlopeRadians, 0.0f, glm::radians(89.0f));
    const float minWalkableNormalDot = std::cos(maxWalkableSlopeRadians);
    const float3 worldUp(0.0f, 1.0f, 0.0f);

    std::vector<JobHandle> handles;
    handles.reserve(cellCount);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            handles.push_back(JobSystem::Instance().Schedule(
                [&, x, y]()
                {
                    const float rayX = originX + (static_cast<float>(x) + 0.5f) * config.resolution.x;
                    const float rayZ = originZ + (static_cast<float>(y) + 0.5f) * config.resolution.y;
                    const float3 rayOrigin(rayX, rayOriginY, rayZ);

                    bool hasHit = false;
                    float nearestDistance = std::numeric_limits<float>::max();
                    float3 nearestNormal(0.0f);

                    for (const BakeMesh& bakeMesh : bakeMeshes)
                    {
                        for (const Submesh& submesh : bakeMesh.mesh->GetSubmeshes())
                        {
                            const std::vector<uint32_t>& indices = submesh.GetIndices();
                            const std::vector<glm::vec3>& positions = submesh.GetPositions();

                            for (int i = 0; i + 2 < submesh.GetIndexCount(); i += 3)
                            {
                                const uint32_t i0 = indices[i];
                                const uint32_t i1 = indices[i + 1];
                                const uint32_t i2 = indices[i + 2];

                                if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size())
                                {
                                    continue;
                                }

                                float3 p0 = float3(bakeMesh.worldMatrix * float4(positions[i0], 1.0f));
                                float3 p1 = float3(bakeMesh.worldMatrix * float4(positions[i1], 1.0f));
                                float3 p2 = float3(bakeMesh.worldMatrix * float4(positions[i2], 1.0f));

                                float2 bary;
                                float distance = 0.0f;
                                if (glm::intersectRayTriangle(rayOrigin, rayDirection, p0, p1, p2, bary, distance) &&
                                    distance >= 0.0f && distance < nearestDistance)
                                {
                                    nearestDistance = distance;
                                    nearestNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                                    hasHit = true;
                                }
                            }
                        }
                    }

                    NavCell& cell = navData.grid.cells[cellIndex(x, y)];
                    cell.valid = false;
                    if (hasHit)
                    {
                        cell.height = rayOrigin.y + rayDirection.y * nearestDistance;
                        cell.valid = std::abs(glm::dot(nearestNormal, worldUp)) >= minWalkableNormalDot;
                    }
                }
            ));
        }
    }

    for (JobHandle& handle : handles)
    {
        handle.Wait();
    }

    handles.clear();

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            handles.push_back(JobSystem::Instance().Schedule(
                [&, x, y]()
                {
                    const int index = cellIndex(x, y);
                    const NavCell& centerCell = navData.grid.cells[index];
                    const float centerHeight = centerCell.height;

                    float4 edgeSlop(0.0f);
                    if (centerCell.valid && x > 0 && navData.grid.cells[cellIndex(x - 1, y)].valid)
                    {
                        edgeSlop.x = std::atan((navData.grid.cells[cellIndex(x - 1, y)].height - centerHeight) / config.resolution.x);
                    }
                    if (centerCell.valid && x + 1 < width && navData.grid.cells[cellIndex(x + 1, y)].valid)
                    {
                        edgeSlop.y = std::atan((navData.grid.cells[cellIndex(x + 1, y)].height - centerHeight) / config.resolution.x);
                    }
                    if (centerCell.valid && y > 0 && navData.grid.cells[cellIndex(x, y - 1)].valid)
                    {
                        edgeSlop.z = std::atan((navData.grid.cells[cellIndex(x, y - 1)].height - centerHeight) / config.resolution.y);
                    }
                    if (centerCell.valid && y + 1 < height && navData.grid.cells[cellIndex(x, y + 1)].valid)
                    {
                        edgeSlop.w = std::atan((navData.grid.cells[cellIndex(x, y + 1)].height - centerHeight) / config.resolution.y);
                    }

                    navData.grid.cells[index].edgeSlop = edgeSlop;
                }
            ));
        }
    }

    for (JobHandle& handle : handles)
    {
        handle.Wait();
    }

    navData.WriteCellsToBinary();
}
