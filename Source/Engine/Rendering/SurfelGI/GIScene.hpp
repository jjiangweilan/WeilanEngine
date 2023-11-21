#pragma once
#include "Core/Asset.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "SurfelBakeFGNode.hpp"

namespace Engine::SurfelGI
{
struct Surfel : Serializable
{
    Surfel() = default;
    Surfel(glm::vec4 albedo, glm::vec4 position, glm::vec4 normal) : albedo(albedo), position(position), normal(normal)
    {}

    ~Surfel() override {}

    glm::vec4 albedo;
    glm::vec4 position;
    glm::vec4 normal;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};

class GIScene : public Asset
{
    DECLARE_ASSET();

public:
    std::vector<Surfel> surfels{};
    GIScene() = default;
    GIScene(GIScene&& other) = default;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};

struct BakerConfig
{
    Scene* scene = nullptr;
    Shader* templateShader = nullptr;
    glm::vec3 worldBoundsMin{};
    glm::vec3 worldBoundsMax{};
    float surfelBoxSize = 1.0f;
};

class GISceneBaker
{
public:
    GIScene Bake(BakerConfig bakerConfig);

private:
    std::vector<glm::vec3> principleNormals = {
        glm::vec3(1, 0, 0),
        glm::vec3(-1, 0, 0),
        glm::vec3(0, 1, 0),
        glm::vec3(0, -1, 0),
        glm::vec3(0, 0, 1),
        glm::vec3(0, 0, -1),
    };
    Camera* bakerCamera;
    Scene* scene;
    FrameGraph::SurfelBakeFGNode* surfelBakeNode;
    UniPtr<Gfx::Buffer> albedoBuf;
    UniPtr<Gfx::Buffer> positionBuf;
    UniPtr<Gfx::Buffer> normalBuf;

    Surfel CaptureSurfel(glm::vec3 center, glm::vec3 principleNormal);
};

} // namespace Engine::SurfelGI
