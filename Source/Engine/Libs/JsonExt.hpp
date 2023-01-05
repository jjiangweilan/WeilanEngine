#pragma once
#include <unordered_map>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
namespace glm
{
    void to_json(nlohmann::json& j, const glm::mat4& v);
    void from_json(const nlohmann::json& j, glm::mat4& v);
}

namespace Engine
{
    struct AssetObjectMeta;
    void to_json(nlohmann::json& j, const Engine::AssetObjectMeta& v);
    void from_json(const nlohmann::json& j, Engine::AssetObjectMeta& v);
}
