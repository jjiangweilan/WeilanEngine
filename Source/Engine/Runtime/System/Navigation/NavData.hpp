#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/Library/Math.hpp"

class BinaryAsset;

struct NavDataConfig
{
    float2 resolution = float2(0.25f); // meter
    float maxWalkableSlopeRadians = 0.7853982f;
    int width = 256;
    int height = 256;
    float3 origin = float3(0.0f);

    INLINE_DEFINE_SERIALIZABLE(
        SER(resolution),
        SER(maxWalkableSlopeRadians),
        SER(width),
        SER(height),
        SER(origin)
    )
};

struct NavCell
{
    float height = 0;
    float4 edgeSlop = float4(0);
    bool valid = true;
};

struct NavGrid
{
    NavDataConfig config;
    std::vector<NavCell> cells;

    INLINE_DEFINE_SERIALIZABLE(
        SER(config)
    )
};

class WEILAN_ENGINE_API NavData : public Asset
{
    DECLARE_ASSET();
    DECLARE_SERIALIZATION()

public:
    NavGrid grid;

    BinaryAsset* GetCellAsset() const;
    void SetCellAsset(BinaryAsset* asset);
    void ClearCells();
    bool WriteCellsToBinary();
    void OnLoaded() override;

private:
    bool ReadCellsFromBinary();

    ObjPtr<BinaryAsset> cellAsset;
};
