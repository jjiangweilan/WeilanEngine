#include <type_traits>
// https://stackoverflow.com/a/17771358/4931426
#pragma once

namespace Engine
{
#define ENUM_FLAGS(T, INT_T)                                                                                           \
    enum class T : INT_T;                                                                                              \
    using T##Flags = T;                                                                                                \
    inline T operator&(T x, T y)                                                                                       \
    {                                                                                                                  \
        return static_cast<T>(static_cast<INT_T>(x) & static_cast<INT_T>(y));                                          \
    };                                                                                                                 \
    inline T operator|(T x, T y)                                                                                       \
    {                                                                                                                  \
        return static_cast<T>(static_cast<INT_T>(x) | static_cast<INT_T>(y));                                          \
    };                                                                                                                 \
    inline T operator^(T x, T y)                                                                                       \
    {                                                                                                                  \
        return static_cast<T>(static_cast<INT_T>(x) ^ static_cast<INT_T>(y));                                          \
    };                                                                                                                 \
    inline T operator~(T x)                                                                                            \
    {                                                                                                                  \
        return static_cast<T>(~static_cast<INT_T>(x));                                                                 \
    };                                                                                                                 \
    inline T& operator&=(T& x, T y)                                                                                    \
    {                                                                                                                  \
        x = x & y;                                                                                                     \
        return x;                                                                                                      \
    };                                                                                                                 \
    inline T& operator|=(T& x, T y)                                                                                    \
    {                                                                                                                  \
        x = x | y;                                                                                                     \
        return x;                                                                                                      \
    };                                                                                                                 \
    inline T& operator^=(T& x, T y)                                                                                    \
    {                                                                                                                  \
        x = x ^ y;                                                                                                     \
        return x;                                                                                                      \
    };                                                                                                                 \
    enum class T : INT_T

template <class T>
inline bool HasFlag(T flags, T val)
{
    return static_cast<std::underlying_type_t<T>>(flags & val) != 0;
}
} // namespace Engine
