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
    if (config.resolution <= 0.0f || config.width <= 0 || config.height <= 0)
    {
        return;
    }

    const int expectedCellCount = config.width * config.height;
    if (grid.cells.size() < expectedCellCount)
    {
        return;
    }

    auto cellIndex = [width = config.width](int x, int y) { return y * width + x; };
    auto cellPosition = [&](int x, int y)
    {
        const NavCell& cell = grid.cells[cellIndex(x, y)];
        return relativePosition + float3(
            (static_cast<float>(x) + 0.5f) * config.resolution,
            cell.height,
            (static_cast<float>(y) + 0.5f) * config.resolution
        );
    };

    constexpr float4 gridColor(0.1f, 1.0f, 0.2f, 1.0f);
    for (int y = 0; y < config.height; ++y)
    {
        for (int x = 0; x < config.width; ++x)
        {
            const float3 center = cellPosition(x, y);
            if (x + 1 < config.width)
            {
                Graphics::DrawLine(center, cellPosition(x + 1, y), gridColor);
            }
            if (y + 1 < config.height)
            {
                Graphics::DrawLine(center, cellPosition(x, y + 1), gridColor);
            }
        }
    }
}
