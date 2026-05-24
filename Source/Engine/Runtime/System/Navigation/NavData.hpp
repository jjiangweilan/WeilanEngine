#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/Library/Math.hpp"

struct NavDataConfig
{
    float resolution = 0.25; // meter
    int width = 256;
    int height = 256;

    INLINE_DEFINE_SERIALIZABLE(
        SER(resolution),
        SER(width),
        SER(height)
    )
};

struct NavCell
{
    float height = 0;
    float4 edgeSlop = float4(0);

    INLINE_DEFINE_SERIALIZABLE(
        SER(height),
        SER(edgeSlop)
    )
};

struct NavGrid
{
    NavDataConfig config;
    std::vector<NavCell> cells;

    INLINE_DEFINE_SERIALIZABLE(
        SER(config),
        SER(cells)
    )
};

class NavData : public Asset
{
    DECLARE_ASSET();
    DECLARE_SERIALIZATION()

public:
    NavGrid grid;
};
