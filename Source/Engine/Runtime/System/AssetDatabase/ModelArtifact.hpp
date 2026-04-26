#pragma once
#include "Engine/Library/PodVector.hpp"
#include "Engine/Library/UUID.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/Animation.hpp"
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>

namespace ModelArtifact
{
bool WriteMeshBlob(const std::filesystem::path& path, const Mesh& mesh);
std::unique_ptr<Mesh> ReadMeshBlob(const PodVector<uint8_t>& data);

bool WriteAnimationBlob(const std::filesystem::path& path, const Animation& animation);
std::unique_ptr<Animation> ReadAnimationBlob(const PodVector<uint8_t>& data);

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
