#pragma once

#include <cmath>
#include <glm/glm.hpp>

namespace ColorSpace
{
inline float SRGBToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

inline float LinearToSRGB(float value)
{
    return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

inline glm::vec3 SRGBToLinear(const glm::vec3& value)
{
    return {SRGBToLinear(value.r), SRGBToLinear(value.g), SRGBToLinear(value.b)};
}

inline glm::vec4 SRGBToLinear(const glm::vec4& value)
{
    return {SRGBToLinear(glm::vec3(value)), value.a};
}

inline glm::vec3 LinearToSRGB(const glm::vec3& value)
{
    return {LinearToSRGB(value.r), LinearToSRGB(value.g), LinearToSRGB(value.b)};
}

inline glm::vec4 LinearToSRGB(const glm::vec4& value)
{
    return {LinearToSRGB(glm::vec3(value)), value.a};
}
} // namespace ColorSpace
