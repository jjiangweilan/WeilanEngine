#pragma once

#include "Math.hpp"
#include <nlohmann/json.hpp>

namespace nlohmann
{
// JSON serialization for float2
inline void to_json(nlohmann::json& j, const float2& v) {
    j = nlohmann::json::array({v.x, v.y});
}

inline void from_json(const nlohmann::json& j, float2& v) {
    v.x = j.at(0).get<float>();
    v.y = j.at(1).get<float>();
}

// JSON serialization for float3
inline void to_json(nlohmann::json& j, const float3& v) {
    j = nlohmann::json::array({v.x, v.y, v.z});
}

inline void from_json(const nlohmann::json& j, float3& v) {
    v.x = j.at(0).get<float>();
    v.y = j.at(1).get<float>();
    v.z = j.at(2).get<float>();
}

// JSON serialization for float4
inline void to_json(nlohmann::json& j, const float4& v) {
    j = nlohmann::json::array({v.x, v.y, v.z, v.w});
}

inline void from_json(const nlohmann::json& j, float4& v) {
    v.x = j.at(0).get<float>();
    v.y = j.at(1).get<float>();
    v.z = j.at(2).get<float>();
    v.w = j.at(3).get<float>();
}

// JSON serialization for float2x2
inline void to_json(nlohmann::json& j, const float2x2& m) {
    j = nlohmann::json::array({
        nlohmann::json::array({m[0][0], m[0][1]}),
        nlohmann::json::array({m[1][0], m[1][1]})
    });
}

inline void from_json(const nlohmann::json& j, float2x2& m) {
    m[0][0] = j.at(0).at(0).get<float>();
    m[0][1] = j.at(0).at(1).get<float>();
    m[1][0] = j.at(1).at(0).get<float>();
    m[1][1] = j.at(1).at(1).get<float>();
}

// JSON serialization for float3x3
inline void to_json(nlohmann::json& j, const float3x3& m) {
    j = nlohmann::json::array({
        nlohmann::json::array({m[0][0], m[0][1], m[0][2]}),
        nlohmann::json::array({m[1][0], m[1][1], m[1][2]}),
        nlohmann::json::array({m[2][0], m[2][1], m[2][2]})
    });
}

inline void from_json(const nlohmann::json& j, float3x3& m) {
    m[0][0] = j.at(0).at(0).get<float>();
    m[0][1] = j.at(0).at(1).get<float>();
    m[0][2] = j.at(0).at(2).get<float>();
    m[1][0] = j.at(1).at(0).get<float>();
    m[1][1] = j.at(1).at(1).get<float>();
    m[1][2] = j.at(1).at(2).get<float>();
    m[2][0] = j.at(2).at(0).get<float>();
    m[2][1] = j.at(2).at(1).get<float>();
    m[2][2] = j.at(2).at(2).get<float>();
}

// JSON serialization for float4x4
inline void to_json(nlohmann::json& j, const float4x4& m) {
    j = nlohmann::json::array({
        nlohmann::json::array({m[0][0], m[0][1], m[0][2], m[0][3]}),
        nlohmann::json::array({m[1][0], m[1][1], m[1][2], m[1][3]}),
        nlohmann::json::array({m[2][0], m[2][1], m[2][2], m[2][3]}),
        nlohmann::json::array({m[3][0], m[3][1], m[3][2], m[3][3]})
    });
}

inline void from_json(const nlohmann::json& j, float4x4& m) {
    m[0][0] = j.at(0).at(0).get<float>();
    m[0][1] = j.at(0).at(1).get<float>();
    m[0][2] = j.at(0).at(2).get<float>();
    m[0][3] = j.at(0).at(3).get<float>();
    m[1][0] = j.at(1).at(0).get<float>();
    m[1][1] = j.at(1).at(1).get<float>();
    m[1][2] = j.at(1).at(2).get<float>();
    m[1][3] = j.at(1).at(3).get<float>();
    m[2][0] = j.at(2).at(0).get<float>();
    m[2][1] = j.at(2).at(1).get<float>();
    m[2][2] = j.at(2).at(2).get<float>();
    m[2][3] = j.at(2).at(3).get<float>();
    m[3][0] = j.at(3).at(0).get<float>();
    m[3][1] = j.at(3).at(1).get<float>();
    m[3][2] = j.at(3).at(2).get<float>();
    m[3][3] = j.at(3).at(3).get<float>();
}

// JSON serialization for int2
inline void to_json(nlohmann::json& j, const int2& v) {
    j = nlohmann::json::array({v.x, v.y});
}

inline void from_json(const nlohmann::json& j, int2& v) {
    v.x = j.at(0).get<int>();
    v.y = j.at(1).get<int>();
}

// JSON serialization for int3
inline void to_json(nlohmann::json& j, const int3& v) {
    j = nlohmann::json::array({v.x, v.y, v.z});
}

inline void from_json(const nlohmann::json& j, int3& v) {
    v.x = j.at(0).get<int>();
    v.y = j.at(1).get<int>();
    v.z = j.at(2).get<int>();
}

// JSON serialization for int4
inline void to_json(nlohmann::json& j, const int4& v) {
    j = nlohmann::json::array({v.x, v.y, v.z, v.w});
}

inline void from_json(const nlohmann::json& j, int4& v) {
    v.x = j.at(0).get<int>();
    v.y = j.at(1).get<int>();
    v.z = j.at(2).get<int>();
    v.w = j.at(3).get<int>();
}

// JSON serialization for uint2
inline void to_json(nlohmann::json& j, const uint2& v) {
    j = nlohmann::json::array({v.x, v.y});
}

inline void from_json(const nlohmann::json& j, uint2& v) {
    v.x = j.at(0).get<unsigned int>();
    v.y = j.at(1).get<unsigned int>();
}

// JSON serialization for uint3
inline void to_json(nlohmann::json& j, const uint3& v) {
    j = nlohmann::json::array({v.x, v.y, v.z});
}

inline void from_json(const nlohmann::json& j, uint3& v) {
    v.x = j.at(0).get<unsigned int>();
    v.y = j.at(1).get<unsigned int>();
    v.z = j.at(2).get<unsigned int>();
}

// JSON serialization for uint4
inline void to_json(nlohmann::json& j, const uint4& v) {
    j = nlohmann::json::array({v.x, v.y, v.z, v.w});
}

inline void from_json(const nlohmann::json& j, uint4& v) {
    v.x = j.at(0).get<unsigned int>();
    v.y = j.at(1).get<unsigned int>();
    v.z = j.at(2).get<unsigned int>();
    v.w = j.at(3).get<unsigned int>();
}

}
