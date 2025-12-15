#pragma once

#include "Component.hpp"
#include "Library/Math/Geometry/Geometry.hpp"
#include "Core/Ptr.hpp"
#include "Library/Math.hpp"

namespace Rendering::FrameGraph
{
class Graph;
}

class Camera : public Component
{
    DECLARE_OBJECT();

public:
    enum class ProjectionMode
    {
        Perspective = 0,
        Orthographic = 1
    };

    Camera();
    Camera(GameObject* gameObject);
    ~Camera() override {};
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    glm::mat4 GetViewMatrix() const;
    void SetViewMatrix(const float4x4& view);
    const glm::mat4& GetAndUpdateProjectionMatrix(float aspect = 0.0f);
    glm::mat4 CalculateProjectionMatrixWithOverride(float farPlane, float aspect = 0.0f);

    float3 ScreenUVToCameraNearPlaneInViewSpace(glm::vec2 screenUV);

    /**
     * @brief This returns the screen UV position when the screen plane in placed in camera's object space
     *
     * @param screenUV [TODO:parameter]
     */
    float3 ScreenUVToCameraNearPlaneInObjectSpace(glm::vec2 screenUV);
    float3 ScreenUVToWorldPos(glm::vec2 screenUV);
    float3 GetForward();
    float3 GetRight();
    float3 GetUp();
    Ray ScreenUVToWorldSpaceRay(glm::vec2 screenUV);
    Frustum GetFrustum(float aspect = 0.0f);

    void SetDiffuseEnv(Texture* cubemap);
    void SetSpecularEnv(Texture* cubemap);
    const SRef<Texture>& GetDiffuseEnv() { return diffuseEnv; }
    const SRef<Texture>& GetSpecularEnv() { return specularEnv; }

    static RefPtr<Camera> mainCamera;

    // get camera fov half angle (returns 0 for Orthographic mode)
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

    // Projection mode and orthographic controls
    ProjectionMode GetProjectionMode() const { return projectionMode; }
    void SetProjectionMode(ProjectionMode mode)
    {
        if (projectionMode != mode)
        {
            projectionMode = mode;
            updateProjectionMatrix = true;
        }
    }

    // Orthographic size describes the full height of the orthographic projection volume
    float GetOrthographicSize() const { return orthographicSize; }
    void SetOrthographicSize(float size)
    {
        if (size != orthographicSize)
        {
            orthographicSize = (size > 0.0f) ? size : 0.0001f;
            updateProjectionMatrix = true;
        }
    }

    void LookAt(const float3& lookAtPos);

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() const override;
    void OnDrawGizmos() override;

private:
    SRef<Texture> diffuseEnv = nullptr;
    SRef<Texture> specularEnv = nullptr;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    float near = 0.01f;
    float far = 25000.0f;
    float fov = glm::radians(60.0);
    float aspect = -1.0f;
    bool updateProjectionMatrix = true;

    ProjectionMode projectionMode = ProjectionMode::Perspective;
    // Orthographic projection height (the width is computed as orthographicSize * aspect)
    float orthographicSize = 10.0f;
};
