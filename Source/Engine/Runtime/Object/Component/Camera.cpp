#include "Camera.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/MiddleLayer/SystemInfo.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include <glm/gtc/matrix_transform.hpp>

DEFINE_OBJECT(Component, Camera, "7BDC1BC9-A96E-4ABC-AE76-DD6AB8C69A19");

TYPE_REFLECTION_MEMBER_VARIABLES(
    Camera,
    TYPE_REFLECTION_MEM(Camera, projectionMatrix),
    TYPE_REFLECTION_MEM(Camera, viewMatrix),
    TYPE_REFLECTION_MEM(Camera, diffuseEnv),
    TYPE_REFLECTION_MEM(Camera, specularEnv)
);
Camera::Camera(GameObject* gameObject) : Component(gameObject), projectionMatrix(), viewMatrix()
{
    if (mainCamera == nullptr)
    {
        mainCamera = this;
    }
}

Camera::Camera() : Component(nullptr), projectionMatrix(), viewMatrix()
{
    if (mainCamera == nullptr)
    {
        mainCamera = this;
    }
};

float Camera::GetProjectionRight()
{
    if (projectionMode == ProjectionMode::Orthographic)
    {
        // Determine aspect if not set
        float a = aspect;
        if (a <= 0.0f)
        {
            auto screenSize = SystemInfo::Singleton().GetScreenResolution();
            a = (screenSize.x != 0.0f && screenSize.y != 0.0f) ? screenSize.x / screenSize.y : 1920.0f / 1080.0f;
        }
        const float orthoWidth = orthographicSize * a;
        return 0.5f * orthoWidth;
    }

    // Perspective: derive from projection matrix
    return near / projectionMatrix[0][0];
}

float Camera::GetProjectionTop()
{
    if (projectionMode == ProjectionMode::Orthographic)
    {
        return 0.5f * orthographicSize;
    }

    // Perspective: derive from projection matrix (negative by convention used elsewhere)
    return -near / projectionMatrix[1][1];
}

float Camera::GetNear()
{
    return near;
}

float Camera::GetFar()
{
    return far;
}

void Camera::SetViewMatrix(const float4x4& view)
{
    auto v = glm::inverse(view);
    v[2] = -v[2];
    gameObject->SetWorldMatrix(v);
}

glm::mat4 Camera::GetViewMatrix() const
{
    auto view = gameObject->GetWorldMatrix();
    view[0] = glm::normalize(view[0]);
    view[1] = glm::normalize(view[1]);
    view[2] = glm::normalize(view[2]);
    view[2] = -view[2];
    view = glm::inverse(view);
    return view;
}

void Camera::SetDiffuseEnv(Texture* cubemap)
{
    if (cubemap)
        diffuseEnv = cubemap->GetSRef<Texture>();
    else
        diffuseEnv = nullptr;
}

void Camera::SetSpecularEnv(Texture* cubemap)
{
    if (cubemap)
        specularEnv = cubemap->GetSRef<Texture>();
    else
        specularEnv = nullptr;
}

const glm::mat4& Camera::GetAndUpdateProjectionMatrix(float aspect)
{
    if (this->aspect != aspect || updateProjectionMatrix)
    {
        if (aspect == 0.0f)
        {
            auto screenSize = SystemInfo::Singleton().GetScreenResolution();
            aspect = (screenSize.x != 0.0f && screenSize.y != 0.0f) ? screenSize.x / screenSize.y : 1920.0f / 1080.0f;
        }

        updateProjectionMatrix = false;
        this->aspect = aspect;

        if (projectionMode == ProjectionMode::Orthographic)
        {
            const float halfHeight = 0.5f * orthographicSize;
            const float halfWidth = 0.5f * (orthographicSize * aspect);
            const float left = -halfWidth;
            const float right = halfWidth;
            const float bottom = -halfHeight;
            const float top = halfHeight;
            projectionMatrix = Math::OrthographicProjectionMatrix(left, right, bottom, top, near, far);
        }
        else
        {
            projectionMatrix = Math::PerspectiveProjectionMatrix(fov, aspect, near, far);
        }
    }

    return projectionMatrix;
}

glm::mat4 Camera::CalculateProjectionMatrixWithOverride(float farPlane, float aspect)
{
    if (aspect == 0.0f)
    {
        auto screenSize = SystemInfo::Singleton().GetScreenResolution();
        aspect = (screenSize.x != 0.0f && screenSize.y != 0.0f) ? screenSize.x / screenSize.y : 1920.0f / 1080.0f;
    }

    if (projectionMode == ProjectionMode::Orthographic)
    {
        const float halfHeight = 0.5f * orthographicSize;
        const float halfWidth = 0.5f * (orthographicSize * aspect);
        const float left = -halfWidth;
        const float right = halfWidth;
        const float bottom = -halfHeight;
        const float top = halfHeight;
        return Math::OrthographicProjectionMatrix(left, right, bottom, top, near, farPlane);
    }

    glm::float4x4 projectionMatrix = Math::PerspectiveProjectionMatrix(fov, aspect, near, farPlane);
    return projectionMatrix;
}

RefPtr<Camera> Camera::mainCamera = nullptr;

glm::vec3 Camera::ScreenUVToCameraNearPlaneInViewSpace(glm::vec2 screenUV)
{
    return glm::vec3(
        (screenUV - glm::vec2(0.5)) * glm::vec2(2) * glm::vec2{GetProjectionRight(), -GetProjectionTop()},
        GetNear()
    );
}

glm::vec3 Camera::ScreenUVToCameraNearPlaneInObjectSpace(glm::vec2 screenUV)
{
    return glm::vec3(
        (screenUV - glm::vec2(0.5)) * glm::vec2(2) * glm::vec2{GetProjectionRight(), -GetProjectionTop()},
        -GetNear()
    );
}

glm::vec3 Camera::ScreenUVToWorldPos(glm::vec2 screenUV)
{
    glm::mat4 camModelMatrix = GetGameObject()->GetWorldMatrix();
    return camModelMatrix * glm::vec4(ScreenUVToCameraNearPlaneInObjectSpace(screenUV), 1.0);
}

Ray Camera::ScreenUVToWorldSpaceRay(glm::vec2 screenUV)
{
    Ray ray;
    glm::mat4 camModelMatrix = GetGameObject()->GetWorldMatrix();
    glm::vec3 viewSpacePosition = ScreenUVToCameraNearPlaneInObjectSpace(screenUV);

    if (projectionMode == ProjectionMode::Orthographic)
    {
        // in orthographic mode, the ray direction is the camera forward direction
        ray.origin = camModelMatrix * glm::vec4(viewSpacePosition, 1.0);
        ray.direction = GetForward();
        return ray;
    }
    else
    {
        glm::vec3 clickInWS = camModelMatrix * glm::vec4(viewSpacePosition, 1.0);
        ray.origin = GetGameObject()->GetPosition();
        ray.direction = glm::normalize(clickInWS - ray.origin);
        return ray;
    }
}

void Camera::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("projectionMatrix", projectionMatrix);
    s->Serialize("viewMatrix", viewMatrix);
    s->Serialize("diffuseEnv", diffuseEnv.Get());
    s->Serialize("specularEnv", specularEnv.Get());
    s->Serialize("near", near);
    s->Serialize("far", far);
    s->Serialize("fov", fov);
    s->Serialize("projectionMode", static_cast<int>(projectionMode));
    s->Serialize("orthographicSize", orthographicSize);
}
void Camera::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("projectionMatrix", projectionMatrix);
    s->Deserialize("viewMatrix", viewMatrix);
    s->Deserialize(
        "diffuseEnv",
        nullptr,
        [this](void* data)
        {
            if (Texture* tex = (Texture*)data)
            {
                diffuseEnv = tex->GetSRef<Texture>();
            }
        }
    );

    s->Deserialize(
        "specularEnv",
        nullptr,
        [this](void* data)
        {
            if (Texture* tex = (Texture*)data)
            {
                specularEnv = tex->GetSRef<Texture>();
            }
        }
    );

    s->Deserialize("near", near);
    s->Deserialize("far", far);
    s->Deserialize("fov", fov);
    int projectionModeValue = static_cast<int>(projectionMode);
    s->Deserialize("projectionMode", projectionModeValue);
    projectionMode = static_cast<ProjectionMode>(projectionModeValue);
    s->Deserialize("orthographicSize", orthographicSize);
    updateProjectionMatrix = true;
}

const std::string& Camera::GetName() const
{
    static std::string name = "Camera";
    return name;
};

std::unique_ptr<Component> Camera::Clone(GameObject& owner)
{
    std::unique_ptr<Camera> clone = std::make_unique<Camera>(&owner);

    clone->enabled = enabled;
    clone->projectionMatrix = projectionMatrix;
    clone->viewMatrix = viewMatrix;
    clone->near = near;
    clone->far = far;
    clone->fov = fov;
    clone->aspect = aspect;
    clone->diffuseEnv = diffuseEnv;
    clone->specularEnv = specularEnv;
    clone->projectionMode = projectionMode;
    clone->orthographicSize = orthographicSize;
    return clone;
}

glm::vec3 Camera::GetForward()
{
    return -gameObject->GetForward();
}

void Camera::OnDrawGizmos()
{
    Gizmos::DrawCamera(gameObject->GetPosition());
}

float Camera::GetFoV()
{
    if (projectionMode == ProjectionMode::Orthographic)
    {
        return 0.0f;
    }
    return fov;
}

Frustum Camera::GetFrustum(float aspect)
{
    auto view = GetViewMatrix();
    auto proj = GetAndUpdateProjectionMatrix(aspect);

    auto vp = proj * view;
    return Frustum(vp);
}

void Camera::LookAt(const float3& lookAtPos)
{
    auto pos = GetGameObject()->GetPosition();
    float3 dir = glm::normalize(lookAtPos - pos);
    auto mat = glm::lookAt(pos, lookAtPos, glm::abs(dir) != float3(0, 1, 0) ? float3(0, 1, 0) : float3(0, 0, 1));
    float3x3 rotMat = mat;
    rotMat = glm::transpose(rotMat);
    auto rot = glm::quat_cast(rotMat);
    GetGameObject()->SetRotation(rot);
}

float3 Camera::GetRight()
{
    return gameObject->GetRight();
}

float3 Camera::GetUp()
{
    return gameObject->GetUp();
}
