#pragma once

#include "ThirdParty/xxHash/xxhash.h"
#include <spdlog/spdlog.h>
#include <string_view>

namespace Gfx
{
class ShaderBindingHandle
{
public:
    ShaderBindingHandle() : hash(0) {}
    ShaderBindingHandle(const std::string_view& name) : hash(XXH3_64bits((void*)name.data(), name.size())) {}

    uint64_t operator()() const { return hash; }

    uint64_t GetHash() const { return hash; }

    bool operator==(const ShaderBindingHandle& other) const = default;

private:
    uint64_t hash;

    friend class std::hash<Gfx::ShaderBindingHandle>;
};
} // namespace Gfx
  //

inline Gfx::ShaderBindingHandle operator""_shaderBinding(const char* c, std::size_t s)
{
    return Gfx::ShaderBindingHandle(std::string_view(c, s));
}

template <>
struct std::hash<Gfx::ShaderBindingHandle>
{
    size_t operator()(Gfx::ShaderBindingHandle handle) const { return std::hash<uint64_t>()(handle.hash); }
};
