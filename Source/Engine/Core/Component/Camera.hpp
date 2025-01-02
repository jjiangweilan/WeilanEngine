#pragma once

#include "Component.hpp"
#include "Core/Math/Geometry.hpp"
#include "Core/Ptr.hpp"
#include "Libs/Math.hpp"

namespace Rendering::FrameGraph
{
class Graph;
}

class Camera : public Component
{
    DECLARE_OBJECT();

public:
    Camera();
    Camera(GameObject* gameObject);
    ~Camera() override {};
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    glm::mat4 GetViewMatrix() const;
    const glm::mat4& GetProjectionMatrix() const;
    glm::vec3 ScreenUVToViewSpace(glm::vec2 screenUV);
    glm::vec3 ScreenUVToWorldPos(glm::vec2 screenUV);
    glm::vec3 GetForward();
    Ray ScreenUVToWorldSpaceRay(glm::vec2 screenUV);
    void SetProjectionMatrix(float fovy, float aspect, float zNear, float zFar);
    void SetProjectionMatrix(const glm::mat4& proj);
    void GenerateFrustumPlanes(glm::float4 frustumPlanes[6]);

    void SetDiffuseEnv(Texture* cubemap);
    void SetSpecularEnv(Texture* cubemap);
    const SRef<Texture>& GetDiffuseEnv() { return diffuseEnv; }
    const SRef<Texture>& GetSpecularEnv() { return specularEnv; }

    void SetFrameGraph(Rendering::FrameGraph::Graph* graph);
    Rendering::FrameGraph::Graph* GetFrameGraph() const { return frameGraph; }

    static RefPtr<Camera> mainCamera;

    // get camera fov half angle
    float GetFoV();
    float GetProjectionRight();
    float GetProjectionTop();
    float GetNear();
    float GetFar();

    void DrawGizmos();
    void Tick() override;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void OnDrawGizmos() override;

private:
    SRef<Texture> diffuseEnv = nullptr;
    SRef<Texture> specularEnv = nullptr;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    ObjPtr<Rendering::FrameGraph::Graph> frameGraph = nullptr;
    float near;
    float far;
    float fov;
    float aspect;
};
