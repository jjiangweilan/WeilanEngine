#pragma once

#include "Component.hpp"
#include "Core/Math/Geometry.hpp"
#include <glm/glm.hpp>
namespace Engine
{
namespace FrameGraph
{
class Graph;
}
class Camera : public Component
{
    DECLARE_OBJECT();

public:
    Camera();
    Camera(GameObject* gameObject);
    ~Camera() override{};
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    glm::mat4 GetViewMatrix() const;
    const glm::mat4& GetProjectionMatrix() const;
    glm::vec3 ScreenUVToViewSpace(glm::vec2 screenUV);
    glm::vec3 ScreenUVToWorldPos(glm::vec2 screenUV);
    Ray ScreenUVToWorldSpaceRay(glm::vec2 screenUV);
    void SetProjectionMatrix(float fovy, float aspect, float zNear, float zFar);
    void SetProjectionMatrix(const glm::mat4& proj);

    void SetFrameGraph(FrameGraph::Graph* graph);
    FrameGraph::Graph* GetFrameGraph() const
    {
        return graph;
    }

    static RefPtr<Camera> mainCamera;

    float GetProjectionRight();
    float GetProjectionTop();
    float GetNear();
    float GetFar();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;

private:
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    FrameGraph::Graph* graph = nullptr;
};
} // namespace Engine
