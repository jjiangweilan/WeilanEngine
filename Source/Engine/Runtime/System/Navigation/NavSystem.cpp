#include "./NavSystem.hpp"

#include "Engine/Runtime/System/Rendering/Graphics.hpp"

void NavSystem::Init(ObjPtr<NavData> data)
{
    navData = data;
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
    std::vector<Graphics::Line> lines;
    lines.reserve((config.width - 1) * config.height + config.width * (config.height - 1));
    for (int y = 0; y < config.height; ++y)
    {
        for (int x = 0; x < config.width; ++x)
        {
            const float3 center = cellPosition(x, y);
            if (x + 1 < config.width)
            {
                lines.push_back({center, cellPosition(x + 1, y), gridColor});
            }
            if (y + 1 < config.height)
            {
                lines.push_back({center, cellPosition(x, y + 1), gridColor});
            }
        }
    }
    Graphics::DrawLines(lines);
}

uint64_t NavObjectHandle::GenerateHandleID()
{
    static uint64_t id = 0;
    return id++;
}
