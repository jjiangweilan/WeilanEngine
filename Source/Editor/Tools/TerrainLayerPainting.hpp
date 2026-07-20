#pragma once

#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace Editor::TerrainLayerPainting
{
struct WorkingSample
{
    std::array<uint8_t, 4> ids;
    std::array<float, 4> weights;
};

struct QuantizedSample
{
    TerrainLayerControlSample ids;
    TerrainLayerControlSample weights;
};

inline std::array<uint8_t, 4> ToArray(const TerrainLayerControlSample& sample)
{
    return {sample.x, sample.y, sample.z, sample.w};
}

inline TerrainLayerControlSample ToSample(const std::array<uint8_t, 4>& values)
{
    return {values[0], values[1], values[2], values[3]};
}

inline WorkingSample CreateWorkingSample(
    const TerrainLayerControlSample& ids,
    const TerrainLayerControlSample& weights
)
{
    WorkingSample working{ToArray(ids), {}};
    const std::array<uint8_t, 4> sourceWeights = ToArray(weights);
    float totalWeight = 0.0f;
    for (size_t channel = 0; channel < working.weights.size(); ++channel)
    {
        working.weights[channel] = working.ids[channel] == TerrainConfig::InvalidTerrainLayerID
                                       ? 0.0f
                                       : static_cast<float>(sourceWeights[channel]);
        totalWeight += working.weights[channel];
    }
    if (totalWeight > 0.0f)
    {
        const float scale = 255.0f / totalWeight;
        for (float& weight : working.weights)
            weight *= scale;
    }
    return working;
}

inline size_t PrepareSelectedLayer(WorkingSample& working, uint8_t selectedLayerID)
{
    for (size_t channel = 0; channel < working.ids.size(); ++channel)
    {
        if (working.ids[channel] == selectedLayerID)
            return channel;
    }
    for (size_t channel = 0; channel < working.ids.size(); ++channel)
    {
        if (working.ids[channel] == TerrainConfig::InvalidTerrainLayerID)
        {
            working.ids[channel] = selectedLayerID;
            working.weights[channel] = 0.0f;
            return channel;
        }
    }

    const auto smallest = std::min_element(working.weights.begin(), working.weights.end());
    const size_t channel = static_cast<size_t>(std::distance(working.weights.begin(), smallest));
    working.ids[channel] = selectedLayerID;
    return channel;
}

inline void IntegrateSelectedLayer(
    WorkingSample& working,
    size_t selectedChannel,
    float speed,
    float deltaTime,
    float influence
)
{
    if (selectedChannel >= working.weights.size() || speed <= 0.0f || deltaTime <= 0.0f || influence <= 0.0f)
        return;

    const float retention = std::exp(-speed * deltaTime * influence);
    float otherWeight = 0.0f;
    for (size_t channel = 0; channel < working.weights.size(); ++channel)
    {
        if (channel == selectedChannel)
            continue;
        working.weights[channel] *= retention;
        otherWeight += working.weights[channel];
    }
    working.weights[selectedChannel] = std::max(255.0f - otherWeight, 0.0f);
}

inline QuantizedSample Quantize(const WorkingSample& working, bool canonicalizeZeroWeights = false)
{
    std::array<uint8_t, 4> ids = working.ids;
    std::array<float, 4> normalizedWeights{};
    float totalWeight = 0.0f;
    for (size_t channel = 0; channel < normalizedWeights.size(); ++channel)
    {
        normalizedWeights[channel] = ids[channel] == TerrainConfig::InvalidTerrainLayerID
                                         ? 0.0f
                                         : std::max(working.weights[channel], 0.0f);
        totalWeight += normalizedWeights[channel];
    }

    std::array<uint8_t, 4> quantizedWeights{};
    if (totalWeight > 0.0f)
    {
        const float scale = 255.0f / totalWeight;
        std::array<float, 4> fractions{};
        uint32_t quantizedTotal = 0;
        for (size_t channel = 0; channel < normalizedWeights.size(); ++channel)
        {
            const float scaledWeight = normalizedWeights[channel] * scale;
            const float flooredWeight = std::floor(scaledWeight);
            quantizedWeights[channel] = static_cast<uint8_t>(flooredWeight);
            fractions[channel] = scaledWeight - flooredWeight;
            quantizedTotal += quantizedWeights[channel];
        }

        while (quantizedTotal < 255)
        {
            size_t bestChannel = 0;
            for (size_t channel = 1; channel < fractions.size(); ++channel)
            {
                if (fractions[channel] > fractions[bestChannel])
                    bestChannel = channel;
            }
            ++quantizedWeights[bestChannel];
            fractions[bestChannel] = -1.0f;
            ++quantizedTotal;
        }
    }

    if (canonicalizeZeroWeights)
    {
        for (size_t channel = 0; channel < ids.size(); ++channel)
        {
            if (quantizedWeights[channel] == 0)
                ids[channel] = TerrainConfig::InvalidTerrainLayerID;
        }
    }
    return {ToSample(ids), ToSample(quantizedWeights)};
}
} // namespace Editor::TerrainLayerPainting
