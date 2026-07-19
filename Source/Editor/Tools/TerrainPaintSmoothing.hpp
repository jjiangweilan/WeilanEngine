#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Editor::TerrainPaintSmoothing
{
inline float SampleHeightBilinear(
    std::span<const uint16_t> samples,
    uint32_t resolution,
    float x,
    float y
)
{
    const float lastTexel = static_cast<float>(resolution - 1);
    x = std::clamp(x, 0.0f, lastTexel);
    y = std::clamp(y, 0.0f, lastTexel);

    const uint32_t x0 = static_cast<uint32_t>(x);
    const uint32_t y0 = static_cast<uint32_t>(y);
    const uint32_t x1 = std::min(x0 + 1, resolution - 1);
    const uint32_t y1 = std::min(y0 + 1, resolution - 1);
    const float fractionX = x - static_cast<float>(x0);
    const float fractionY = y - static_cast<float>(y0);

    const float topLeft = static_cast<float>(samples[static_cast<size_t>(y0) * resolution + x0]);
    const float topRight = static_cast<float>(samples[static_cast<size_t>(y0) * resolution + x1]);
    const float bottomLeft = static_cast<float>(samples[static_cast<size_t>(y1) * resolution + x0]);
    const float bottomRight = static_cast<float>(samples[static_cast<size_t>(y1) * resolution + x1]);
    const float top = topLeft + (topRight - topLeft) * fractionX;
    const float bottom = bottomLeft + (bottomRight - bottomLeft) * fractionX;
    return top + (bottom - top) * fractionY;
}

inline float CalculateAdaptiveKernelStep(
    uint32_t heightMapResolution,
    uint32_t vertexResolution,
    float brushRadiusTexels
)
{
    const float lastTexel = static_cast<float>(heightMapResolution - 1);
    const float vertexInterval = lastTexel / static_cast<float>(vertexResolution - 1);
    return std::min(std::max(vertexInterval, brushRadiusTexels * 0.25f), lastTexel);
}

inline float CalculateSmoothedHeight(
    std::span<const uint16_t> samples,
    uint32_t resolution,
    float centerX,
    float centerY,
    float stepX,
    float stepY
)
{
    constexpr float KernelWeights[3] = {1.0f, 2.0f, 1.0f};
    float weightedHeight = 0.0f;
    float totalWeight = 0.0f;
    for (int kernelY = -1; kernelY <= 1; ++kernelY)
    {
        for (int kernelX = -1; kernelX <= 1; ++kernelX)
        {
            const float weight = KernelWeights[kernelX + 1] * KernelWeights[kernelY + 1];
            weightedHeight += SampleHeightBilinear(
                                  samples,
                                  resolution,
                                  centerX + static_cast<float>(kernelX) * stepX,
                                  centerY + static_cast<float>(kernelY) * stepY
                              ) *
                              weight;
            totalWeight += weight;
        }
    }
    return weightedHeight / totalWeight;
}

inline float IntegrateSmoothedHeight(
    float currentHeight,
    float targetHeight,
    float smoothingRate,
    float deltaTime,
    float brushWeight
)
{
    const float blend = 1.0f - std::exp(-smoothingRate * deltaTime * brushWeight);
    return currentHeight + (targetHeight - currentHeight) * blend;
}
} // namespace Editor::TerrainPaintSmoothing
