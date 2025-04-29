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
    const glm::mat4& GetAndUpdateProjectionMatrix(float aspect = 0.0f);
    glm::mat4 CalculateProjectionMatrixWithOverride(float farPlane, float aspect = 0.0f);
    glm::vec3 ScreenUVToViewSpace(glm::vec2 screenUV);
    glm::vec3 ScreenUVToWorldPos(glm::vec2 screenUV);
    glm::vec3 GetForward();
    Ray ScreenUVToWorldSpaceRay(glm::vec2 screenUV);
    Frustum GetFrustum(float aspect = 0.0f);

    void SetDiffuseEnv(Texture* cubemap);
    void SetSpecularEnv(Texture* cubemap);
    const SRef<Texture>& GetDiffuseEnv() { return diffuseEnv; }
    const SRef<Texture>& GetSpecularEnv() { return specularEnv; }

    static RefPtr<Camera> mainCamera;

    // get camera fov half angle
    float GetFoV();
    float GetProjectionRight();
    float GetProjectionTop();
    float GetNear();
    float GetFar();
    void SetFoV(float fov)
    {
        this->fov = fov;
        updateProjectionMatrix = true;
    }
    void SetNear(float near)
    {
        this->near = near;
        updateProjectionMatrix = true;
    }
    void SetFar(float far)
    {
        this->far = far;
        updateProjectionMatrix = true;
    }

    void LookAt(const float3& lookAtPos);

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void OnDrawGizmos() override;

private:
    SRef<Texture> diffuseEnv = nullptr;
    SRef<Texture> specularEnv = nullptr;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    float near = 0.01f;
    float far = 1000.0f;
    float fov = glm::radians(60.0);
    float aspect = -1.0f;
    bool updateProjectionMatrix = true;
};
