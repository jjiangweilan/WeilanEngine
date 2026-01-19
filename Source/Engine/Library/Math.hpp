#pragma once

#include "Random.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>
#include <spdlog/spdlog.h> // to extend spdlog
                           //

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_EXT_INCLUDED)
#ifndef GLM_ENABLE_EXPERIMENTAL
#pragma message(                                                                                                                                                                   \
    "GLM: GLM_GTX_compatibility is an experimental extension and may change in the future. Use #define GLM_ENABLE_EXPERIMENTAL before including it, if you really want to use it." \
)
#else
#pragma message("GLM: GLM_GTX_compatibility extension included")
#endif
#endif

#if GLM_COMPILER & GLM_COMPILER_VC
#include <cfloat>
#elif GLM_COMPILER & GLM_COMPILER_GCC
#include <cmath>
#if (GLM_PLATFORM & GLM_PLATFORM_ANDROID)
#undef isfinite
#endif
#endif // GLM_COMPILER

namespace glm
{
template <class Type, int R, int C>
using matrix = mat<R, C, Type, highp>;

typedef bool bool1;                //!< \brief boolean type with 1 component. (From GLM_GTX_compatibility extension)
typedef vec<2, bool, highp> bool2; //!< \brief boolean type with 2 components. (From GLM_GTX_compatibility extension)
typedef vec<3, bool, highp> bool3; //!< \brief boolean type with 3 components. (From GLM_GTX_compatibility extension)
typedef vec<4, bool, highp> bool4; //!< \brief boolean type with 4 components. (From GLM_GTX_compatibility extension)

typedef bool bool1x1; //!< \brief boolean matrix with 1 x 1 component. (From GLM_GTX_compatibility extension)
typedef mat<2, 2, bool, highp>
    bool2x2; //!< \brief boolean matrix with 2 x 2 components. (From GLM_GTX_compatibility extension)
typedef mat<2, 3, bool, highp>
    bool2x3; //!< \brief boolean matrix with 2 x 3 components. (From GLM_GTX_compatibility extension)
typedef mat<2, 4, bool, highp>
    bool2x4; //!< \brief boolean matrix with 2 x 4 components. (From GLM_GTX_compatibility extension)
typedef mat<3, 2, bool, highp>
    bool3x2; //!< \brief boolean matrix with 3 x 2 components. (From GLM_GTX_compatibility extension)
typedef mat<3, 3, bool, highp>
    bool3x3; //!< \brief boolean matrix with 3 x 3 components. (From GLM_GTX_compatibility extension)
typedef mat<3, 4, bool, highp>
    bool3x4; //!< \brief boolean matrix with 3 x 4 components. (From GLM_GTX_compatibility extension)
typedef mat<4, 2, bool, highp>
    bool4x2; //!< \brief boolean matrix with 4 x 2 components. (From GLM_GTX_compatibility extension)
typedef mat<4, 3, bool, highp>
    bool4x3; //!< \brief boolean matrix with 4 x 3 components. (From GLM_GTX_compatibility extension)
typedef mat<4, 4, bool, highp>
    bool4x4; //!< \brief boolean matrix with 4 x 4 components. (From GLM_GTX_compatibility extension)
             //

typedef int int1;                  //!< \brief integer vector with 1 component. (From GLM_GTX_compatibility extension)
typedef vec<2, int, highp> int2;   //!< \brief integer vector with 2 components. (From GLM_GTX_compatibility extension)
typedef vec<3, int, highp> int3;   //!< \brief integer vector with 3 components. (From GLM_GTX_compatibility extension)
typedef vec<4, int, highp> int4;   //!< \brief integer vector with 4 components. (From GLM_GTX_compatibility extension)
                                   //
typedef vec<2, uint, highp> uint2; //!< \brief integer vector with 2 components. (From GLM_GTX_compatibility extension)
typedef vec<3, uint, highp> uint3; //!< \brief integer vector with 3 components. (From GLM_GTX_compatibility extension)
typedef vec<4, uint, highp> uint4; //!< \brief integer vector with 4 components. (From GLM_GTX_compatibility extension)

typedef int int1x1; //!< \brief integer matrix with 1 component. (From GLM_GTX_compatibility extension)
typedef mat<2, 2, int, highp>
    int2x2; //!< \brief integer matrix with 2 x 2 components. (From GLM_GTX_compatibility extension)
typedef mat<2, 3, int, highp>
    int2x3; //!< \brief integer matrix with 2 x 3 components. (From GLM_GTX_compatibility extension)
typedef mat<2, 4, int, highp>
    int2x4; //!< \brief integer matrix with 2 x 4 components. (From GLM_GTX_compatibility extension)
typedef mat<3, 2, int, highp>
    int3x2; //!< \brief integer matrix with 3 x 2 components. (From GLM_GTX_compatibility extension)
typedef mat<3, 3, int, highp>
    int3x3; //!< \brief integer matrix with 3 x 3 components. (From GLM_GTX_compatibility extension)
typedef mat<3, 4, int, highp>
    int3x4; //!< \brief integer matrix with 3 x 4 components. (From GLM_GTX_compatibility extension)
typedef mat<4, 2, int, highp>
    int4x2; //!< \brief integer matrix with 4 x 2 components. (From GLM_GTX_compatibility extension)
typedef mat<4, 3, int, highp>
    int4x3; //!< \brief integer matrix with 4 x 3 components. (From GLM_GTX_compatibility extension)
typedef mat<4, 4, int, highp>
    int4x4; //!< \brief integer matrix with 4 x 4 components. (From GLM_GTX_compatibility extension)
            //

typedef float
    float1; //!< \brief single-qualifier floating-point vector with 1 component. (From GLM_GTX_compatibility extension)
typedef vec<2, float, highp>
    float2; //!< \brief single-qualifier floating-point vector with 2 components. (From GLM_GTX_compatibility extension)
typedef vec<3, float, highp>
    float3; //!< \brief single-qualifier floating-point vector with 3 components. (From GLM_GTX_compatibility extension)
typedef vec<4, float, highp>
    float4; //!< \brief single-qualifier floating-point vector with 4 components. (From GLM_GTX_compatibility extension)

typedef float float1x1;                   //!< \brief single-qualifier floating-point matrix with 1 component. (From GLM_GTX_compatibility
                                          //!< extension)
typedef mat<2, 2, float, highp> float2x2; //!< \brief single-qualifier floating-point matrix with 2 x 2 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<2, 3, float, highp> float2x3; //!< \brief single-qualifier floating-point matrix with 2 x 3 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<2, 4, float, highp> float2x4; //!< \brief single-qualifier floating-point matrix with 2 x 4 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<3, 2, float, highp> float3x2; //!< \brief single-qualifier floating-point matrix with 3 x 2 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<3, 3, float, highp> float3x3; //!< \brief single-qualifier floating-point matrix with 3 x 3 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<3, 4, float, highp> float3x4; //!< \brief single-qualifier floating-point matrix with 3 x 4 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<4, 2, float, highp> float4x2; //!< \brief single-qualifier floating-point matrix with 4 x 2 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<4, 3, float, highp> float4x3; //!< \brief single-qualifier floating-point matrix with 4 x 3 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<4, 4, float, highp> float4x4; //!< \brief single-qualifier floating-point matrix with 4 x 4 components.
                                          //!< (From GLM_GTX_compatibility extension)
typedef mat<4, 4, float, highp> float4x4; //!< \brief single-qualifier floating-point matrix with 4 x 4 components.
                                          //!< (From GLM_GTX_compatibility extension)

typedef double
    double1;                           //!< \brief double-qualifier floating-point vector with 1 component. (From GLM_GTX_compatibility extension)
typedef vec<2, double, highp> double2; //!< \brief double-qualifier floating-point vector with 2 components. (From
                                       //!< GLM_GTX_compatibility extension)
typedef vec<3, double, highp> double3; //!< \brief double-qualifier floating-point vector with 3 components. (From
                                       //!< GLM_GTX_compatibility extension)
typedef vec<4, double, highp> double4; //!< \brief double-qualifier floating-point vector with 4 components. (From
                                       //!< GLM_GTX_compatibility extension)

typedef double double1x1;                   //!< \brief double-qualifier floating-point matrix with 1 component. (From
                                            //!< GLM_GTX_compatibility extension)
typedef mat<2, 2, double, highp> double2x2; //!< \brief double-qualifier floating-point matrix with 2 x 2 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<2, 3, double, highp> double2x3; //!< \brief double-qualifier floating-point matrix with 2 x 3 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<2, 4, double, highp> double2x4; //!< \brief double-qualifier floating-point matrix with 2 x 4 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<3, 2, double, highp> double3x2; //!< \brief double-qualifier floating-point matrix with 3 x 2 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<3, 3, double, highp> double3x3; //!< \brief double-qualifier floating-point matrix with 3 x 3 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<3, 4, double, highp> double3x4; //!< \brief double-qualifier floating-point matrix with 3 x 4 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<4, 2, double, highp> double4x2; //!< \brief double-qualifier floating-point matrix with 4 x 2 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<4, 3, double, highp> double4x3; //!< \brief double-qualifier floating-point matrix with 4 x 3 components.
                                            //!< (From GLM_GTX_compatibility extension)
typedef mat<4, 4, double, highp> double4x4; //!< \brief double-qualifier floating-point matrix with 4 x 4 components.
                                            //!< (From GLM_GTX_compatibility extension)

/// @}
} // namespace glm

using glm::float2;
using glm::float2x2;
using glm::float3;
using glm::float3x3;
using glm::float4;
using glm::float4x4;
using glm::int2;
using glm::int3;
using glm::int4;
using glm::uint2;
using glm::uint3;
using glm::uint4;

namespace Math
{
float4x4 PerspectiveProjectionMatrix(float fov, float aspect, float near, float far);
float4x4 OrthographicProjectionMatrix(float left, float right, float bottom, float top, float near, float far);

void DecomposeMatrix(const glm::mat4& m, glm::vec3& pos, glm::vec3& scale, glm::quat& rot);

template <std::unsigned_integral T>
bool IsPowerOfTwo(T value)
{
    return (value & (value - 1)) == 0;
}

template <std::unsigned_integral T>
T RoundToAlignmentPoT(T address, T alignment)
{
    return ((address + (alignment - 1)) & ~(alignment - 1));
}

template <std::unsigned_integral T>
T AlignMemory(T offset, T alignment)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

} // namespace Math
  //

// spdlog extension

// Specialize fmt::formatter
template <>
struct fmt::formatter<float4>
{
    constexpr auto parse(const format_parse_context& ctx) const -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const float4& v, const FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[float4]({:.4f}, {:.4f}, {:.4f}, {:.4f})", v.x, v.y, v.z, v.w);
    }
};

template <>
struct fmt::formatter<float3>
{
    constexpr auto parse(const format_parse_context& ctx) const -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const float3& v, const FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[float3]({:.4f}, {:.4f}, {:.4f})", v.x, v.y, v.z);
    }
};

template <>
struct fmt::formatter<float2>
{
    constexpr auto parse(const format_parse_context& ctx) const -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const float2& v, const FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[float2]({:.4f}, {:.4f})", v.x, v.y);
    }
};

template <>
struct fmt::formatter<int2>
{
    constexpr auto parse(const format_parse_context& ctx) const -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const int2& v, const FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[int2]({}, {})", v.x, v.y);
    }
};

template <>
struct fmt::formatter<glm::quat>
{
    constexpr auto parse(const format_parse_context& ctx) const -> decltype(ctx.begin()) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const glm::quat& v, const FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[quaternion]({:.4f}, {:.4f}, {:.4f}, {:.4f})", v.x, v.y, v.z, v.w);
    }
};

struct Offset2D
{
    int32_t x;
    int32_t y;
};

struct Offset3D
{
    int32_t x;
    int32_t y;
    int32_t z;
};

struct Extent2D
{
    uint32_t width;
    uint32_t height;
};

struct Extent3D
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

struct Rect2D
{
    Offset2D offset;
    Extent2D extent;
};

struct AABB
{
    struct PosConstruct
    {};
    glm::float3 min = {};
    glm::float3 max = {};
    glm::float3 GetCenter() const { return (min + max) / 2.0f; }

    AABB() : min(0), max(0) {}
    AABB(const glm::float3& min, const glm::float3& max) : min(min), max(max) {}
    AABB(const glm::float3& center, const glm::float3& size, PosConstruct)
    {
        glm::float3 halfSize = size / 2.0f;
        min = center - halfSize;
        max = center + halfSize;
    }

    void Transform(const glm::float3x3& rs, const glm::float3& t)
    {

        // fast path
        // the following algorithm only works for uniform scaling
        if (rs[0] == glm::float3(1, 0, 0) && rs[1] == glm::float3(0, 1, 0) && rs[2] == glm::float3(0, 0, 1))
        {
            glm::float3 nmin, nmax;
            nmin = t;
            nmax = t;

            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                {
                    float a = rs[i][j] * min[j];
                    float b = rs[i][j] * max[j];
                    nmin[i] += a < b ? a : b;
                    nmax[i] += a < b ? b : a;
                }

            min = nmin;
            max = nmax;
        }
        else
        {
            auto m = glm::float4x4(rs);
            m[3] = glm::float4(t, 1);
            glm::float3 nmin, nmax;
            nmin = glm::float3(std::numeric_limits<float>::max());
            nmax = glm::float3(std::numeric_limits<float>::lowest());

            glm::float3 v[] = {
                {min.x, min.y, min.z},
                {max.x, min.y, min.z},
                {min.x, max.y, min.z},
                {min.x, min.y, max.z},

                {max.x, max.y, min.z},
                {min.x, max.y, max.z},
                {max.x, min.y, max.z},
                {max.x, max.y, max.z},
            };

            for (int vi = 0; vi < 8; vi++)
            {
                v[vi] = m * glm::float4(v[vi], 1.0);
            }

            for (int vi = 0; vi < 8; vi++)
            {
                nmin = glm::min(nmin, v[vi]);
                nmax = glm::max(nmax, v[vi]);
            }

            min = nmin;
            max = nmax;
        }
    }
};
