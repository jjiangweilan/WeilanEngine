#pragma once
#include "Engine/Library/UUID.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <optional>
#include <string_view>

namespace AssetArtifacts
{
enum class Kind
{
    Model,
    Mesh,
    Texture,
    Material,
    AnimationClip,
    AnimationSet,
};

struct Type
{
    Kind kind;
    std::string_view name;
    std::string_view extension;
};

inline constexpr Type Model{Kind::Model, "model", ".model"};
inline constexpr Type Mesh{Kind::Mesh, "mesh", ".meshblob"};
inline constexpr Type Texture{Kind::Texture, "texture", ".ktx"};
inline constexpr Type Material{Kind::Material, "material", ".matblob"};
inline constexpr Type AnimationClip{Kind::AnimationClip, "animationClip", ".animclipblob"};
inline constexpr Type AnimationSet{Kind::AnimationSet, "animationSet", ".animsetblob"};

inline constexpr Type GetType(Kind kind)
{
    switch (kind)
    {
        case Kind::Model: return Model;
        case Kind::Mesh: return Mesh;
        case Kind::Texture: return Texture;
        case Kind::Material: return Material;
        case Kind::AnimationClip: return AnimationClip;
        case Kind::AnimationSet: return AnimationSet;
    }

    return Model;
}

inline constexpr std::string_view ToString(Kind kind)
{
    return GetType(kind).name;
}

inline constexpr std::string_view Extension(Kind kind)
{
    return GetType(kind).extension;
}

inline std::optional<Kind> FromString(std::string_view name)
{
    if (name == Model.name)
        return Kind::Model;
    if (name == Mesh.name)
        return Kind::Mesh;
    if (name == Texture.name)
        return Kind::Texture;
    if (name == Material.name)
        return Kind::Material;
    if (name == AnimationClip.name)
        return Kind::AnimationClip;
    if (name == AnimationSet.name)
        return Kind::AnimationSet;

    return std::nullopt;
}

inline UUID MakeArtifactUUID(const UUID& sourceAssetUUID, Kind kind, std::string_view locator)
{
    return UUID(fmt::format("{}:{}:{}", sourceAssetUUID.ToString(), ToString(kind), locator), UUID::FromStrTag{});
}

inline std::filesystem::path MakeArtifactPath(const UUID& artifactUUID, Kind kind)
{
    return std::filesystem::path(artifactUUID.ToString()).replace_extension(Extension(kind));
}
} // namespace AssetArtifacts
