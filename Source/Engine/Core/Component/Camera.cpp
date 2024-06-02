#include "Camera.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp" // serializaiton
#include <glm/gtc/matrix_transform.hpp>
DEFINE_OBJECT(Camera, "7BDC1BC9-A96E-4ABC-AE76-DD6AB8C69A19");
Camera::Camera(GameObject* gameObject) : Component(gameObject), projectionMatrix(), viewMatrix()
{
    if (mainCamera == nullptr)
    {
        mainCamera = this;
    }

    auto swapChainImage = GetGfxDriver()->GetSwapChainImage();
    auto& desc = swapChainImage->GetDescription();
    SetProjectionMatrix(glm::radians(60.0f), desc.width / (float)desc.height, 0.01f, 1000.f);
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
    auto view = glm::inverse(gameObject->GetModelMatrix());
    return view;
}

void Camera::SetProjectionMatrix(float fovy, float aspect, float n, float f)
{
    // float tangent = glm::tan(fovy / 2);
    // float t = n * tangent;
    // float r = t * aspect;
    //
    // glm::mat4 proj(0.0f);
    // proj[0][0] = n / r;
    // proj[1][1] = -n / t;
    // proj[2][2] = f / (f - n);
    // proj[2][3] = 1;
    // proj[3][2] = -f * n / (f - n);
    projectionMatrix = glm::perspectiveLH_ZO(fovy, aspect, n, f);
    projectionMatrix[1] = -projectionMatrix[1];

    this->aspect = aspect;
    fov = fovy;
    near = n;
    far = f;
}

const glm::mat4& Camera::GetProjectionMatrix() const
{
    return projectionMatrix;
}

RefPtr<Camera> Camera::mainCamera = nullptr;

glm::vec3 Camera::ScreenUVToViewSpace(glm::vec2 screenUV)
{
    return glm::vec3(
        (screenUV - glm::vec2(0.5)) * glm::vec2(2) * glm::vec2{GetProjectionRight(), -GetProjectionTop()},
        GetNear()
    );
}

glm::vec3 Camera::ScreenUVToWorldPos(glm::vec2 screenUV)
{
    glm::mat4 camModelMatrix = GetGameObject()->GetModelMatrix();
    return camModelMatrix * glm::vec4(ScreenUVToViewSpace(screenUV), 1.0);
}

Ray Camera::ScreenUVToWorldSpaceRay(glm::vec2 screenUV)
{
    Ray ray;
    ray.origin = GetGameObject()->GetPosition();
    glm::mat4 camModelMatrix = GetGameObject()->GetModelMatrix();
    glm::vec3 clickInWS = camModelMatrix * glm::vec4(ScreenUVToViewSpace(screenUV), 1.0);
    ray.direction = clickInWS - ray.origin;
    return ray;
}

void Camera::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("projectionMatrix", projectionMatrix);
    s->Serialize("viewMatrix", viewMatrix);
    s->Serialize("frameGraph", frameGraph);
}
void Camera::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("projectionMatrix", projectionMatrix);
    s->Deserialize("viewMatrix", viewMatrix);
    s->Deserialize("frameGraph", frameGraph);
}

void Camera::SetProjectionMatrix(const glm::mat4& proj)
{
    this->projectionMatrix = proj;
}

const std::string& Camera::GetName()
{
    static std::string name = "Camera";
    return name;
};

std::unique_ptr<Component> Camera::Clone(GameObject& owner)
{
    std::unique_ptr<Camera> clone = std::make_unique<Camera>(&owner);

    clone->projectionMatrix = projectionMatrix;
    clone->viewMatrix = viewMatrix;
    return clone;
}

void Camera::SetFrameGraph(Rendering::FrameGraph::Graph* graph)
{
    this->frameGraph = graph;
}

glm::vec3 Camera::GetForward()
{
    return -gameObject->GetForward();
}

void Camera::OnDrawGizmos()
{
    // gizmos.Add<GizmoCamera>();
}
