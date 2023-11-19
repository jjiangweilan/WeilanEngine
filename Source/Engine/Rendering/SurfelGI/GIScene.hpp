#pragma once
#include "Core/Scene/Scene.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"

namespace Engine::SurfelGI
{
class GIScene
{
public:
    friend class GISceneBaker;
};

struct BakerConfig
{
    Scene* scene = nullptr;
    Shader* templateShader = nullptr;
    glm::vec3 worldBoundsMin{};
    glm::vec3 worldBoundsMax{};
};

class GISceneBaker
{
public:
    void Bake(BakerConfig bakerConfig);
};
} // namespace Engine::SurfelGI
