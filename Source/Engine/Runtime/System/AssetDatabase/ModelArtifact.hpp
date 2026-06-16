#pragma once
#include "Engine/WeilanEngineAPI.hpp"
#include "Engine/Library/PodVector.hpp"
#include "Engine/Library/UUID.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/AnimationClip.hpp"
#include "Engine/Runtime/System/Rendering/AnimationSet.hpp"
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>

namespace ModelArtifact
{
WEILAN_ENGINE_API bool WriteMeshBlob(const std::filesystem::path& path, const Mesh& mesh);
WEILAN_ENGINE_API std::unique_ptr<Mesh> ReadMeshBlob(const PodVector<uint8_t>& data);

WEILAN_ENGINE_API bool WriteAnimationClipBlob(const std::filesystem::path& path, const AnimationClip& clip);
WEILAN_ENGINE_API std::unique_ptr<AnimationClip> ReadAnimationClipBlob(const PodVector<uint8_t>& data);
WEILAN_ENGINE_API bool WriteAnimationSetBlob(const std::filesystem::path& path, const AnimationSet& animationSet);
WEILAN_ENGINE_API std::unique_ptr<AnimationSet> ReadAnimationSetBlob(const PodVector<uint8_t>& data);

bool WriteModelGraph(
    const std::filesystem::path& path,
    const std::vector<std::unique_ptr<GameObject>>& gameObjects,
    const std::vector<ObjPtr<GameObject>>& roots
);
bool ReadModelGraph(
    const PodVector<uint8_t>& data,
    std::vector<std::unique_ptr<GameObject>>& gameObjects,
    std::vector<ObjPtr<GameObject>>& roots
);
} // namespace ModelArtifact
