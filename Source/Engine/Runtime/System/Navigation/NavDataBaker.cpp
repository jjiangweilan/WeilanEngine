#include "NavDataBaker.hpp"

#include "Engine/Core/JobSystem.hpp"

#include <glm/gtx/intersect.hpp>

#include <cmath>
#include <limits>
#include <vector>

void NavDataBaker::Bake(Mesh* mesh, NavData& navData)
{
    const NavDataConfig& config = navData.grid.config;

    if (mesh == nullptr || config.resolution <= 0 || config.width <= 0 || config.height <= 0)
    {
        navData.grid.cells.clear();
        return;
    }

    const int width = config.width;
    const int height = config.height;
    const int cellCount = width * height;
    navData.grid.cells.clear();
    navData.grid.cells.resize(cellCount);

    auto cellIndex = [width](int x, int y) { return y * width + x; };
    const auto& submeshes = mesh->GetSubmeshes();
    const float rayOriginY = mesh->GetAABB().max.y + 1.0f;
    const float3 rayDirection(0.0f, -1.0f, 0.0f);

    std::vector<JobHandle> handles;
    handles.reserve(cellCount);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            handles.push_back(JobSystem::Instance().Schedule(
                [&, x, y]()
                {
                    const float rayX = (static_cast<float>(x) + 0.5f) * config.resolution;
                    const float rayZ = (static_cast<float>(y) + 0.5f) * config.resolution;
                    const float3 rayOrigin(rayX, rayOriginY, rayZ);

                    bool hasHit = false;
                    float nearestDistance = std::numeric_limits<float>::max();

                    for (const Submesh& submesh : submeshes)
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

                            float2 bary;
                            float distance = 0.0f;
                            if (glm::intersectRayTriangle(
                                    rayOrigin,
                                    rayDirection,
                                    positions[i0],
                                    positions[i1],
                                    positions[i2],
                                    bary,
                                    distance
                                ) &&
                                distance >= 0.0f && distance < nearestDistance)
                            {
                                nearestDistance = distance;
                                hasHit = true;
                            }
                        }
                    }

                    if (hasHit)
                    {
                        navData.grid.cells[cellIndex(x, y)].height = rayOrigin.y + rayDirection.y * nearestDistance;
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
                    const float centerHeight = navData.grid.cells[index].height;

                    float4 edgeSlop(0.0f);
                    if (x > 0)
                    {
                        edgeSlop.x = std::atan((navData.grid.cells[cellIndex(x - 1, y)].height - centerHeight) / config.resolution);
                    }
                    if (x + 1 < width)
                    {
                        edgeSlop.y = std::atan((navData.grid.cells[cellIndex(x + 1, y)].height - centerHeight) / config.resolution);
                    }
                    if (y > 0)
                    {
                        edgeSlop.z = std::atan((navData.grid.cells[cellIndex(x, y - 1)].height - centerHeight) / config.resolution);
                    }
                    if (y + 1 < height)
                    {
                        edgeSlop.w = std::atan((navData.grid.cells[cellIndex(x, y + 1)].height - centerHeight) / config.resolution);
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
}
