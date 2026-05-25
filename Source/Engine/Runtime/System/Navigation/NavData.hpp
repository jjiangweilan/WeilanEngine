#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/Library/Math.hpp"

struct NavDataConfig
{
    float2 resolution = float2(0.25f); // meter
    int width = 256;
    int height = 256;
    float3 origin = float3(0.0f);

    INLINE_DEFINE_SERIALIZABLE(
        SER(resolution),
        SER(width),
        SER(height),
        SER(origin)
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
