#pragma once

#include "ThirdParty/xxHash/xxhash.h"
#include <spdlog/spdlog.h>
#include <string_view>

namespace Gfx
{
class ResourceHandle
{
public:
    ResourceHandle() : hash(0) {}
    ResourceHandle(const std::string_view& name) : hash(XXH3_64bits((void*)name.data(), name.size()))
    {
        // spdlog::info("hash {}, result {}, size {}", name, hash, name.size());
    }

    uint64_t operator()() const
    {
        return hash;
    }

    uint64_t GetHash() const
    {
        return hash;
    }

    bool operator==(const ResourceHandle& other) const = default;

private:
    uint64_t hash;

    friend class std::hash<Gfx::ResourceHandle>;
};
} // namespace Gfx
  //

template <>
struct std::hash<Gfx::ResourceHandle>
{
    size_t operator()(Gfx::ResourceHandle handle) const
    {
        return std::hash<uint64_t>()(handle.hash);
    }
};
