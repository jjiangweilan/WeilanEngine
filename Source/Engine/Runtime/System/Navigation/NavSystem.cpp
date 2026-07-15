#include "./NavSystem.hpp"

#include "Engine/Core/JobSystem.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "Engine/Library/Random.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

#include <glm/gtx/norm.hpp>

namespace
{
NavSystem* globalNavSystem = nullptr;
}

float3 NavPathResult::GetWaypoint(int index) const
{
    if (index < 0 || index >= static_cast<int>(waypoints.size()))
        return float3(0.0f);

    return waypoints[index];
}

struct NavSystem::AStarPathfinder
{
    struct Node
    {
        float gCost = std::numeric_limits<float>::max();
        float hCost = 0.0f;
        int parent = -1;
        bool closed = false;
        bool opened = false;
    };

    struct QueueEntry
    {
        int index = -1;
        float fCost = 0.0f;
        float hCost = 0.0f;
    };

    struct QueueCompare
    {
        bool operator()(const QueueEntry& a, const QueueEntry& b) const
        {
            if (a.fCost != b.fCost)
                return a.fCost > b.fCost;
            if (a.hCost != b.hCost)
                return a.hCost > b.hCost;
            return a.index > b.index;
        }
    };

    NavSystem& navSystem;
    const NavDataConfig& config;
    const NavPathQuery& query;
    std::vector<Node> nodes;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueCompare> openSet;

    AStarPathfinder(NavSystem& navSystem, const NavDataConfig& config, const NavPathQuery& query)
        : navSystem(navSystem), config(config), query(query), nodes(static_cast<size_t>(config.width * config.height))
    {
    }

    NavPathResult Find(const float3& startWorld, const float3& endWorld)
    {
        int2 startCell;
        int2 endCell;
        if (!navSystem.WorldToCell(startWorld, startCell) || !navSystem.WorldToCell(endWorld, endCell))
            return {};

        if (!navSystem.IsCellWalkable(startCell) || !navSystem.IsCellWalkable(endCell))
            return {};

        if (startCell == endCell)
            return {.success = true, .waypoints = {navSystem.CellToWorldCenter(startCell)}};

        const int startIndex = navSystem.CellIndex(startCell);
        const int endIndex = navSystem.CellIndex(endCell);
        nodes[startIndex].gCost = 0.0f;
        nodes[startIndex].hCost = Heuristic(startCell, endCell);
        nodes[startIndex].opened = true;
        openSet.push({startIndex, nodes[startIndex].hCost, nodes[startIndex].hCost});

        while (!openSet.empty())
        {
            const QueueEntry currentEntry = openSet.top();
            openSet.pop();

            Node& currentNode = nodes[currentEntry.index];
            if (currentNode.closed)
                continue;

            currentNode.closed = true;
            if (currentEntry.index == endIndex)
                return BuildResult(startIndex, endIndex);

            const int2 currentCell(currentEntry.index % config.width, currentEntry.index / config.width);
            VisitNeighbors(currentCell, endCell);
        }

        return {};
    }

    void VisitNeighbors(int2 currentCell, int2 endCell)
    {
        static constexpr std::array<int2, 8> directions = {
            int2(-1, 0),
            int2(1, 0),
            int2(0, -1),
            int2(0, 1),
            int2(-1, -1),
            int2(1, -1),
            int2(-1, 1),
            int2(1, 1),
        };

        const int currentIndex = navSystem.CellIndex(currentCell);
        for (const int2& direction : directions)
        {
            if (!query.allowDiagonal && direction.x != 0 && direction.y != 0)
                continue;

            const int2 neighborCell = currentCell + direction;
            if (!navSystem.CanMoveBetween(currentCell, neighborCell, query.maxSlopeRadians))
                continue;

            const int neighborIndex = navSystem.CellIndex(neighborCell);
            Node& neighborNode = nodes[neighborIndex];
            if (neighborNode.closed)
                continue;

            const float tentativeGCost = nodes[currentIndex].gCost + MovementCost(currentCell, neighborCell);
            if (neighborNode.opened && tentativeGCost >= neighborNode.gCost)
                continue;

            neighborNode.gCost = tentativeGCost;
            neighborNode.hCost = Heuristic(neighborCell, endCell);
            neighborNode.parent = currentIndex;
            neighborNode.opened = true;
            openSet.push({neighborIndex, neighborNode.gCost + neighborNode.hCost, neighborNode.hCost});
        }
    }

    float MovementCost(int2 from, int2 to) const
    {
        const float dx = static_cast<float>(std::abs(to.x - from.x)) * config.resolution.x;
        const float dz = static_cast<float>(std::abs(to.y - from.y)) * config.resolution.y;
        const float baseCost = std::sqrt(dx * dx + dz * dz);
        const float heightDelta = std::abs(navSystem.CellToWorldCenter(to).y - navSystem.CellToWorldCenter(from).y);
        return baseCost + heightDelta;
    }

    float Heuristic(int2 from, int2 to) const
    {
        const float dx = static_cast<float>(std::abs(to.x - from.x)) * config.resolution.x;
        const float dz = static_cast<float>(std::abs(to.y - from.y)) * config.resolution.y;
        const float diagonalCost = std::sqrt(config.resolution.x * config.resolution.x + config.resolution.y * config.resolution.y);
        const float diagonalSteps = std::min(dx / config.resolution.x, dz / config.resolution.y);
        const float straightX = dx - diagonalSteps * config.resolution.x;
        const float straightZ = dz - diagonalSteps * config.resolution.y;
        return query.allowDiagonal ? diagonalSteps * diagonalCost + straightX + straightZ : dx + dz;
    }

    NavPathResult BuildResult(int startIndex, int endIndex) const
    {
        std::vector<int2> cells;
        for (int index = endIndex; index != -1; index = nodes[index].parent)
        {
            cells.push_back(int2(index % config.width, index / config.width));
            if (index == startIndex)
                break;
        }

        if (cells.empty() || navSystem.CellIndex(cells.back()) != startIndex)
            return {};

        std::reverse(cells.begin(), cells.end());
        std::vector<float3> waypoints;
        if (query.smoothPath)
        {
            waypoints = navSystem.SmoothPath(cells, query.maxSlopeRadians);
        }
        else
        {
            waypoints.reserve(cells.size());
            for (const int2& cell : cells)
            {
                waypoints.push_back(navSystem.CellToWorldCenter(cell));
            }
        }

        return {.success = !waypoints.empty(), .waypoints = std::move(waypoints)};
    }
};

void NavSystem::Init(ObjPtr<NavData> data)
{
    navData = data;
    runtimeCells.clear();
    if (navData)
    {
        runtimeCells.reserve(navData->grid.cells.size());
        for (auto& cell : navData->grid.cells)
        {
            runtimeCells.push_back({cell, false, 0});
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
            if (!grid.cells[index].valid)
                continue;

            const float3 center = cellPosition(x, y);
            if (x + 1 < config.width && grid.cells[cellIndex(x + 1, y)].valid)
            {
                lines.push_back({center, cellPosition(x + 1, y), gridColor});
            }
            if (y + 1 < config.height && grid.cells[cellIndex(x, y + 1)].valid)
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

NavPathResult NavSystem::FindPath(const float3& startWorld, const float3& endWorld, const NavPathQuery& query)
{
    if (!EnsureRuntimeCells())
        return {};

    NavData* data = navData.Get();
    AStarPathfinder pathfinder(*this, data->grid.config, query);
    return pathfinder.Find(startWorld, endWorld);
}

NavPathResult NavSystem::Lua_FindPath(const float3& startWorld, const float3& endWorld)
{
    return FindPath(startWorld, endWorld);
}

float3 NavSystem::PossionSampleFreeArea(const float3& position, float size, float outterRadius, float innerRadius)
{
    if (!EnsureRuntimeCells())
        return position;

    outterRadius = std::max(0.0f, outterRadius);
    innerRadius = std::clamp(innerRadius, 0.0f, outterRadius);

    const float objectRadius = std::max(0.0f, size * 0.5f);
    if (outterRadius <= 0.0f)
    {
        return position;
    }

    if (innerRadius <= 0.0f && IsAreaFree(position, objectRadius))
        return position;

    NavData* data = navData.Get();
    const NavDataConfig& config = data->grid.config;
    const float minSampleDistance = std::max(std::min(config.resolution.x, config.resolution.y), std::max(size, 0.01f));
    const float minSampleDistanceSq = minSampleDistance * minSampleDistance;
    constexpr int candidatesPerSample = 16;
    constexpr int maxSamples = 128;
    constexpr float twoPi = 6.28318530717958647692f;

    std::vector<float3> samples;
    std::vector<int> active;
    samples.reserve(maxSamples);
    active.reserve(maxSamples);

    auto isInsideSearchArea = [&](const float3& candidate)
    {
        const float distanceSq = glm::distance2(float2(position.x, position.z), float2(candidate.x, candidate.z));
        return distanceSq >= innerRadius * innerRadius && distanceSq <= outterRadius * outterRadius;
    };

    auto hasPoissonSpacing = [&](const float3& candidate)
    {
        for (const float3& sample : samples)
        {
            if (glm::distance2(float2(candidate.x, candidate.z), float2(sample.x, sample.z)) < minSampleDistanceSq)
                return false;
        }
        return true;
    };

    auto makeCandidate = [&](const float3& center)
    {
        const float angle = Random::GenerateNormalized() * twoPi;
        const float radius = minSampleDistance * (1.0f + Random::GenerateNormalized());
        float3 candidate = center + float3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        int2 cell;
        if (WorldToCell(candidate, cell))
        {
            const float3 cellCenter = CellToWorldCenter(cell);
            candidate.y = cellCenter.y;
        }
        return candidate;
    };

    auto makeSearchCandidate = [&]()
    {
        const float angle = Random::GenerateNormalized() * twoPi;
        const float innerSq = innerRadius * innerRadius;
        const float radius = std::sqrt(innerSq + Random::GenerateNormalized() * (outterRadius * outterRadius - innerSq));
        float3 candidate = position + float3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        int2 cell;
        if (WorldToCell(candidate, cell))
        {
            const float3 cellCenter = CellToWorldCenter(cell);
            candidate.y = cellCenter.y;
        }
        return candidate;
    };

    for (int i = 0; i < maxSamples / 4; ++i)
    {
        const float3 candidate = makeSearchCandidate();
        if (!isInsideSearchArea(candidate))
            continue;

        if (IsAreaFree(candidate, objectRadius))
            return candidate;

        if (!hasPoissonSpacing(candidate))
            continue;

        samples.push_back(candidate);
        active.push_back(static_cast<int>(samples.size()) - 1);
    }

    while (!active.empty() && static_cast<int>(samples.size()) < maxSamples)
    {
        const int activeIndex = std::min(static_cast<int>(Random::GenerateNormalized() * static_cast<float>(active.size())), static_cast<int>(active.size()) - 1);
        const float3 center = samples[active[activeIndex]];
        bool accepted = false;

        for (int i = 0; i < candidatesPerSample; ++i)
        {
            const float3 candidate = makeCandidate(center);
            if (!isInsideSearchArea(candidate) || !hasPoissonSpacing(candidate))
                continue;

            samples.push_back(candidate);
            active.push_back(static_cast<int>(samples.size()) - 1);
            accepted = true;

            if (IsAreaFree(candidate, objectRadius))
                return candidate;

            if (static_cast<int>(samples.size()) >= maxSamples)
                break;
        }

        if (!accepted)
        {
            active[activeIndex] = active.back();
            active.pop_back();
        }
    }

    return position;
}

bool NavSystem::GetSteeringTarget(const std::vector<float3>& waypoints, const float3& currentPosition, const NavSteeringQuery& query, float3& outTarget) const
{
    if (waypoints.empty())
        return false;

    int targetIndex = static_cast<int>(waypoints.size()) - 1;
    for (int i = 0; i < static_cast<int>(waypoints.size()); ++i)
    {
        if (glm::distance(float2(currentPosition.x, currentPosition.z), float2(waypoints[i].x, waypoints[i].z)) > query.waypointReachDistance)
        {
            targetIndex = i;
            break;
        }
    }

    float lookAhead = 0.0f;
    for (int i = targetIndex; i + 1 < static_cast<int>(waypoints.size()); ++i)
    {
        const float segmentLength = glm::distance(float2(waypoints[i].x, waypoints[i].z), float2(waypoints[i + 1].x, waypoints[i + 1].z));
        if (lookAhead + segmentLength >= query.lookAheadDistance)
        {
            const float t = segmentLength > 0.0f ? (query.lookAheadDistance - lookAhead) / segmentLength : 0.0f;
            outTarget = waypoints[i] + (waypoints[i + 1] - waypoints[i]) * t;
            return true;
        }
        lookAhead += segmentLength;
    }

    outTarget = waypoints.back();
    return true;
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

    RegisteredNavObject registered{};
    registered.go = gameObject;
    RefreshNavObjectMeshes(registered);

    AABB worldAabb;
    if (CalculateObjectWorldAABB(registered, worldAabb))
    {
        ApplyNavObjectOccupancy(registered, worldAabb, 1);
        registered.previousWorldAabb = worldAabb;
        registered.hasPreviousOccupancy = true;
    }

    registeredNavObjects[handle.handleId] = std::move(registered);

    return handle;
}

void NavSystem::UpdateRuntimeNavObject(NavObjectHandle handle)
{
    auto it = registeredNavObjects.find(handle.handleId);
    if (it == registeredNavObjects.end())
        return;

    if (it->second.hasPreviousOccupancy)
    {
        ApplyNavObjectOccupancy(it->second, it->second.previousWorldAabb, -1);
        it->second.hasPreviousOccupancy = false;
    }

    RefreshNavObjectMeshes(it->second);
    AABB worldAabb;
    if (CalculateObjectWorldAABB(it->second, worldAabb))
    {
        ApplyNavObjectOccupancy(it->second, worldAabb, 1);
        it->second.previousWorldAabb = worldAabb;
        it->second.hasPreviousOccupancy = true;
    }
}

void NavSystem::RemoveNavObject(NavObjectHandle handle)
{
    auto it = registeredNavObjects.find(handle.handleId);
    if (it == registeredNavObjects.end())
        return;

    if (it->second.hasPreviousOccupancy)
    {
        ApplyNavObjectOccupancy(it->second, it->second.previousWorldAabb, -1);
    }
    registeredNavObjects.erase(it);
}

bool NavSystem::EnsureRuntimeCells()
{
    NavData* data = navData.Get();
    if (data == nullptr)
        return false;

    const NavGrid& grid = data->grid;
    const NavDataConfig& config = grid.config;
    if (config.resolution.x <= 0.0f || config.resolution.y <= 0.0f || config.width <= 0 || config.height <= 0)
        return false;

    const int expectedCellCount = config.width * config.height;
    if (grid.cells.size() < expectedCellCount)
        return false;

    if (runtimeCells.size() != expectedCellCount)
    {
        runtimeCells.clear();
        runtimeCells.reserve(expectedCellCount);
        for (int i = 0; i < expectedCellCount; ++i)
        {
            runtimeCells.push_back({grid.cells[i], false, 0});
        }
    }
    else
    {
        for (int i = 0; i < expectedCellCount; ++i)
        {
            runtimeCells[i].cell = grid.cells[i];
        }
    }

    return true;
}

void NavSystem::RefreshNavObjectMeshes(RegisteredNavObject& registered)
{
    registered.meshRenderers.clear();
    if (registered.go == nullptr)
        return;

    std::vector<MeshRenderer*> renderers = registered.go->GetComponentsInChildren<MeshRenderer>();
    registered.meshRenderers.reserve(renderers.size());
    for (MeshRenderer* renderer : renderers)
    {
        if (renderer == nullptr || renderer->GetGameObject() == nullptr)
            continue;

        registered.meshRenderers.push_back({renderer, renderer->GetGameObject()->GetWorldMatrix()});
    }
}

bool NavSystem::CalculateObjectWorldAABB(const RegisteredNavObject& registered, AABB& outWorldAabb) const
{
    NavData* data = navData.Get();
    if (data == nullptr)
        return false;

    const NavDataConfig& config = data->grid.config;
    const float sphereRadius = std::min(config.resolution.x, config.resolution.y) * 0.5f;

    bool hasAabb = false;
    AABB objectAabb;
    for (const RegisteredMeshRenderer& renderer : registered.meshRenderers)
    {
        if (renderer.renderer == nullptr)
            continue;

        AABB rendererAabb = renderer.renderer->GetAABB();
        if (!hasAabb)
        {
            objectAabb = rendererAabb;
            hasAabb = true;
        }
        else
        {
            objectAabb.min = glm::min(objectAabb.min, rendererAabb.min);
            objectAabb.max = glm::max(objectAabb.max, rendererAabb.max);
        }
    }

    if (!hasAabb)
        return false;

    objectAabb.min.x -= sphereRadius;
    objectAabb.min.z -= sphereRadius;
    objectAabb.max.x += sphereRadius;
    objectAabb.max.z += sphereRadius;
    outWorldAabb = objectAabb;
    return true;
}

bool NavSystem::GetCandidateCellRange(const AABB& worldAabb, CandidateCellRange& outRange) const
{
    NavData* data = navData.Get();
    if (data == nullptr)
        return false;

    const NavDataConfig& config = data->grid.config;
    if (config.resolution.x <= 0.0f || config.resolution.y <= 0.0f || config.width <= 0 || config.height <= 0)
        return false;

    const float invResolutionX = 1.0f / config.resolution.x;
    const float invResolutionY = 1.0f / config.resolution.y;
    const float gridOriginX = relativePosition.x + config.origin.x;
    const float gridOriginZ = relativePosition.z + config.origin.z;

    const int minX = static_cast<int>(std::floor((worldAabb.min.x - gridOriginX) * invResolutionX));
    const int maxX = static_cast<int>(std::floor((worldAabb.max.x - gridOriginX) * invResolutionX));
    const int minY = static_cast<int>(std::floor((worldAabb.min.z - gridOriginZ) * invResolutionY));
    const int maxY = static_cast<int>(std::floor((worldAabb.max.z - gridOriginZ) * invResolutionY));
    if (maxX < 0 || minX >= config.width || maxY < 0 || minY >= config.height)
        return false;

    outRange.minX = std::clamp(minX, 0, config.width - 1);
    outRange.maxX = std::clamp(maxX, 0, config.width - 1);
    outRange.minY = std::clamp(minY, 0, config.height - 1);
    outRange.maxY = std::clamp(maxY, 0, config.height - 1);

    return outRange.minX <= outRange.maxX && outRange.minY <= outRange.maxY;
}

void NavSystem::ApplyNavObjectOccupancy(const RegisteredNavObject& registered, const AABB& worldAabb, int delta)
{
    if (!EnsureRuntimeCells())
        return;

    CandidateCellRange range;
    if (!GetCandidateCellRange(worldAabb, range))
        return;

    NavData* data = navData.Get();
    const NavDataConfig& config = data->grid.config;
    const int width = config.width;
    const float3 origin = config.origin;
    const float2 resolution = config.resolution;

    const float sphereRadius = std::min(resolution.x, resolution.y) * 0.5f;
    const float3 castDir(0.0f, 1.0f, 0.0f);

    struct MeshTestData
    {
        std::vector<ObjPtr<Mesh>> meshes;
        float4x4 worldMatrix;
    };

    std::vector<MeshTestData> testData;
    testData.reserve(registered.meshRenderers.size());
    for (const RegisteredMeshRenderer& renderer : registered.meshRenderers)
    {
        if (renderer.renderer == nullptr)
            continue;

        MeshTestData& data = testData.emplace_back();
        data.worldMatrix = renderer.previousWorldMatrix;
        std::span<ObjPtr<Mesh>> meshes = renderer.renderer->GetMeshes();
        data.meshes.assign(meshes.begin(), meshes.end());
    }

    std::vector<JobHandle> handles;
    handles.reserve((range.maxX - range.minX + 1) * (range.maxY - range.minY + 1));
    for (int y = range.minY; y <= range.maxY; ++y)
    {
        for (int x = range.minX; x <= range.maxX; ++x)
        {
            handles.push_back(JobSystem::Instance().Schedule(
                [&, x, y]()
                {
                    const int index = y * width + x;
                    const float3 cellCenter(
                        relativePosition.x + origin.x + (static_cast<float>(x) + 0.5f) * resolution.x,
                        relativePosition.y + runtimeCells[index].cell.height,
                        relativePosition.z + origin.z + (static_cast<float>(y) + 0.5f) * resolution.y
                    );

                    Sphere sphere{cellCenter, sphereRadius};
                    bool hit = false;
                    for (const MeshTestData& rendererData : testData)
                    {
                        for (const ObjPtr<Mesh>& meshPtr : rendererData.meshes)
                        {
                            Mesh* mesh = meshPtr.Get();
                            if (mesh == nullptr)
                                continue;

                            for (const Submesh& submesh : mesh->GetSubmeshes())
                            {
                                float distance = 0.0f;
                                if (SphereVsMesh(sphere, castDir, &submesh, rendererData.worldMatrix, distance))
                                {
                                    hit = true;
                                    break;
                                }
                            }

                            if (hit)
                                break;
                        }

                        if (hit)
                            break;
                    }

                    if (hit)
                    {
                        runtimeCells[index].occupiedCount = std::max(0, runtimeCells[index].occupiedCount + delta);
                        runtimeCells[index].occupied = runtimeCells[index].occupiedCount > 0;
                    }
                }
            ));
        }
    }

    for (JobHandle& handle : handles)
    {
        handle.Wait();
    }
}

bool NavSystem::IsCellInBounds(int2 cell) const
{
    NavData* data = navData.Get();
    if (data == nullptr)
        return false;

    const NavDataConfig& config = data->grid.config;
    return cell.x >= 0 && cell.y >= 0 && cell.x < config.width && cell.y < config.height;
}

int NavSystem::CellIndex(int2 cell) const
{
    NavData* data = navData.Get();
    return cell.y * data->grid.config.width + cell.x;
}

bool NavSystem::WorldToCell(const float3& world, int2& outCell) const
{
    NavData* data = navData.Get();
    if (data == nullptr)
        return false;

    const NavDataConfig& config = data->grid.config;
    if (config.resolution.x <= 0.0f || config.resolution.y <= 0.0f)
        return false;

    const float gridOriginX = relativePosition.x + config.origin.x;
    const float gridOriginZ = relativePosition.z + config.origin.z;
    outCell.x = static_cast<int>(std::floor((world.x - gridOriginX) / config.resolution.x));
    outCell.y = static_cast<int>(std::floor((world.z - gridOriginZ) / config.resolution.y));
    return IsCellInBounds(outCell);
}

float3 NavSystem::CellToWorldCenter(int2 cell) const
{
    NavData* data = navData.Get();
    const NavDataConfig& config = data->grid.config;
    const RuntimeNavData& runtimeCell = runtimeCells[CellIndex(cell)];
    return relativePosition + config.origin + float3((static_cast<float>(cell.x) + 0.5f) * config.resolution.x, runtimeCell.cell.height, (static_cast<float>(cell.y) + 0.5f) * config.resolution.y);
}

bool NavSystem::IsCellWalkable(int2 cell) const
{
    if (!IsCellInBounds(cell))
        return false;

    const int index = CellIndex(cell);
    return index >= 0 && index < static_cast<int>(runtimeCells.size()) && runtimeCells[index].cell.valid &&
           !runtimeCells[index].occupied;
}

bool NavSystem::IsAreaFree(const float3& world, float radius) const
{
    int2 centerCell;
    if (!WorldToCell(world, centerCell) || !IsCellWalkable(centerCell))
        return false;

    NavData* data = navData.Get();
    if (data == nullptr)
        return false;

    const NavDataConfig& config = data->grid.config;
    const float gridOriginX = relativePosition.x + config.origin.x;
    const float gridOriginZ = relativePosition.z + config.origin.z;
    const float minX = world.x - radius;
    const float maxX = world.x + radius;
    const float minZ = world.z - radius;
    const float maxZ = world.z + radius;
    const int minCellX = static_cast<int>(std::floor((minX - gridOriginX) / config.resolution.x));
    const int maxCellX = static_cast<int>(std::floor((maxX - gridOriginX) / config.resolution.x));
    const int minCellY = static_cast<int>(std::floor((minZ - gridOriginZ) / config.resolution.y));
    const int maxCellY = static_cast<int>(std::floor((maxZ - gridOriginZ) / config.resolution.y));

    if (minCellX < 0 || minCellY < 0 || maxCellX >= config.width || maxCellY >= config.height)
        return false;

    const float radiusSq = radius * radius;
    for (int y = minCellY; y <= maxCellY; ++y)
    {
        for (int x = minCellX; x <= maxCellX; ++x)
        {
            const int2 cell(x, y);
            const float3 cellCenter = CellToWorldCenter(cell);
            const float halfWidth = config.resolution.x * 0.5f;
            const float halfHeight = config.resolution.y * 0.5f;
            const float closestX = std::clamp(world.x, cellCenter.x - halfWidth, cellCenter.x + halfWidth);
            const float closestZ = std::clamp(world.z, cellCenter.z - halfHeight, cellCenter.z + halfHeight);
            const float dx = world.x - closestX;
            const float dz = world.z - closestZ;
            if (dx * dx + dz * dz <= radiusSq && !IsCellWalkable(cell))
                return false;
        }
    }

    return true;
}

bool NavSystem::CanMoveBetween(int2 from, int2 to, float maxSlopeRadians) const
{
    if (!IsCellWalkable(from) || !IsCellWalkable(to))
        return false;

    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    if (dx == 0 && dy == 0)
        return true;

    if (std::abs(dx) > 1 || std::abs(dy) > 1)
        return false;

    auto cardinalPassable = [&](int2 a, int2 b)
    {
        if (!IsCellWalkable(a) || !IsCellWalkable(b))
            return false;

        const int cardinalDx = b.x - a.x;
        const int cardinalDy = b.y - a.y;
        if (std::abs(cardinalDx) + std::abs(cardinalDy) != 1)
            return false;

        const NavCell& fromCell = runtimeCells[CellIndex(a)].cell;
        const NavCell& toCell = runtimeCells[CellIndex(b)].cell;
        float fromSlope = 0.0f;
        float toSlope = 0.0f;
        if (cardinalDx < 0)
        {
            fromSlope = fromCell.edgeSlop.x;
            toSlope = toCell.edgeSlop.y;
        }
        else if (cardinalDx > 0)
        {
            fromSlope = fromCell.edgeSlop.y;
            toSlope = toCell.edgeSlop.x;
        }
        else if (cardinalDy < 0)
        {
            fromSlope = fromCell.edgeSlop.z;
            toSlope = toCell.edgeSlop.w;
        }
        else
        {
            fromSlope = fromCell.edgeSlop.w;
            toSlope = toCell.edgeSlop.z;
        }

        return std::abs(fromSlope) <= maxSlopeRadians && std::abs(toSlope) <= maxSlopeRadians;
    };

    if (dx == 0 || dy == 0)
        return cardinalPassable(from, to);

    const int2 horizontalCell(to.x, from.y);
    const int2 verticalCell(from.x, to.y);
    return cardinalPassable(from, horizontalCell) && cardinalPassable(from, verticalCell) && cardinalPassable(horizontalCell, to) && cardinalPassable(verticalCell, to);
}

bool NavSystem::HasLineOfSight(int2 from, int2 to, float maxSlopeRadians) const
{
    if (!IsCellWalkable(from) || !IsCellWalkable(to))
        return false;

    int2 current = from;
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    const int nx = std::abs(dx);
    const int ny = std::abs(dy);
    const int stepX = dx > 0 ? 1 : -1;
    const int stepY = dy > 0 ? 1 : -1;

    int ix = 0;
    int iy = 0;
    while (ix < nx || iy < ny)
    {
        int2 next = current;
        const int decision = (1 + 2 * ix) * ny - (1 + 2 * iy) * nx;
        if (decision == 0)
        {
            next.x += stepX;
            next.y += stepY;
            ++ix;
            ++iy;
        }
        else if (decision < 0)
        {
            next.x += stepX;
            ++ix;
        }
        else
        {
            next.y += stepY;
            ++iy;
        }

        if (!CanMoveBetween(current, next, maxSlopeRadians))
            return false;

        current = next;
    }

    return true;
}

std::vector<float3> NavSystem::SmoothPath(const std::vector<int2>& cells, float maxSlopeRadians) const
{
    std::vector<float3> waypoints;
    if (cells.empty())
        return waypoints;

    waypoints.push_back(CellToWorldCenter(cells.front()));
    size_t anchor = 0;
    while (anchor + 1 < cells.size())
    {
        size_t nextAnchor = anchor + 1;
        for (size_t candidate = cells.size() - 1; candidate > anchor + 1; --candidate)
        {
            if (HasLineOfSight(cells[anchor], cells[candidate], maxSlopeRadians))
            {
                nextAnchor = candidate;
                break;
            }
        }

        waypoints.push_back(CellToWorldCenter(cells[nextAnchor]));
        anchor = nextAnchor;
    }

    return waypoints;
}
