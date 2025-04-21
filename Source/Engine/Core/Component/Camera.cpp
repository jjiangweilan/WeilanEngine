#include "Camera.hpp"
#include "Core/GameObject.hpp"
#include "Core/SystemInfo.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <glm/gtc/matrix_transform.hpp>

DEFINE_OBJECT(Camera, "7BDC1BC9-A96E-4ABC-AE76-DD6AB8C69A19");
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
    return near / projectionMatrix[0][0];
}

float Camera::GetProjectionTop()
{
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

glm::mat4 Camera::GetViewMatrix() const
{
    auto view = gameObject->GetWorldMatrix();
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
            auto screenSize = SystemInfo::Singleton().GetScreenSize();
            aspect = (screenSize.x != 0.0f && screenSize.y != 0.0f) ? screenSize.x / screenSize.y : 1920.0f / 1080.0f;
        }

        updateProjectionMatrix = false;
        this->aspect = aspect;
        projectionMatrix = glm::perspectiveLH_ZO(fov, aspect, near, far);
        projectionMatrix[1] = -projectionMatrix[1];
    }

    return projectionMatrix;
}

RefPtr<Camera> Camera::mainCamera = nullptr;

glm::vec3 Camera::ScreenUVToViewSpace(glm::vec2 screenUV)
{
    return glm::vec3(
        (screenUV - glm::vec2(0.5)) * glm::vec2(2) * glm::vec2{GetProjectionRight(), -GetProjectionTop()},
        -GetNear()
    );
}

glm::vec3 Camera::ScreenUVToWorldPos(glm::vec2 screenUV)
{
    glm::mat4 camModelMatrix = GetGameObject()->GetWorldMatrix();
    return camModelMatrix * glm::vec4(ScreenUVToViewSpace(screenUV), 1.0);
}

Ray Camera::ScreenUVToWorldSpaceRay(glm::vec2 screenUV)
{
    Ray ray;
    ray.origin = GetGameObject()->GetPosition();
    glm::mat4 camModelMatrix = GetGameObject()->GetWorldMatrix();
    glm::vec3 viewSpacePosition = ScreenUVToViewSpace(screenUV);
    glm::vec3 clickInWS = camModelMatrix * glm::vec4(viewSpacePosition, 1.0);
    ray.direction = glm::normalize(clickInWS - ray.origin);
    return ray;
}

void Camera::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("projectionMatrix", projectionMatrix);
    s->Serialize("viewMatrix", viewMatrix);
    s->Serialize("diffuseEnv", diffuseEnv.Get());
    s->Serialize("specularEnv", specularEnv.Get());
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
}

const std::string& Camera::GetName()
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
    return clone;
}

glm::vec3 Camera::GetForward()
{
    return -gameObject->GetForward();
}

void Camera::OnDrawGizmos()
{
    // gizmos.Add<GizmoCamera>();
}

float Camera::GetFoV()
{
    return glm::atan(GetProjectionTop() / GetNear());
}

void Camera::GetFrustumPlanes(glm::float4 frustumPlanes[6])
{
    auto view = GetViewMatrix();
    auto proj = GetAndUpdateProjectionMatrix();

    auto vp = proj * view;
    auto row3 = glm::row(vp, 3);
    auto row0 = glm::row(vp, 0);
    auto row1 = glm::row(vp, 1);
    auto row2 = glm::row(vp, 2);

    // Left plane
    frustumPlanes[0] = row3 + row0;
    // Right plane
    frustumPlanes[1] = row3 - row0;
    // Bottom plane
    frustumPlanes[2] = row3 + row1;
    // Top plane
    frustumPlanes[3] = row3 - row1;
    // Near plane
    frustumPlanes[4] = row2; // z ranges from 0 to 1
    // Far plane
    frustumPlanes[5] = row3 - row2;

    for (int i = 0; i < 6; ++i)
    {
        float length = glm::length(glm::vec3(frustumPlanes[i]));
        frustumPlanes[i] /= length;
    }
}

void Camera::Tick() {}

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
