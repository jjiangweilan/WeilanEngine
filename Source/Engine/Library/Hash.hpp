#pragma once
#include <cstddef>
#include "ThirdParty/xxHash/xxh3.h"

template <class T>
inline void Hash64(uint64_t& seed, const T& v)
{
    seed ^= XXH3_64bits(&v, sizeof(T)) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

inline void Hash64(uint64_t& seed, void* data, size_t size)
{
    seed ^= XXH3_64bits(data, size) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}
