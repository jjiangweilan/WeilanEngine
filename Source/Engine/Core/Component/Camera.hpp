#pragma once

#include "Component.hpp"
#include "Core/Math/Geometry.hpp"
#include <glm/glm.hpp>
namespace Engine
{
class Camera : public Component
{
public:
    Camera();
    Camera(GameObject* gameObject);
    ~Camera() override{};
    glm::mat4 GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
    glm::vec3 ScreenUVToViewSpace(glm::vec2 screenUV);
    glm::vec3 ScreenUVToWorldPos(glm::vec2 screenUV);
    Ray ScreenUVToWorldSpaceRay(glm::vec2 screenUV);

    static RefPtr<Camera> mainCamera;

    float GetProjectionRight();
    float GetProjectionTop();
    float GetNear();
    float GetFar();

private:
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;

    friend class SerializableField<Camera>;
};
template <>
struct SerializableField<Camera>
{
    static void Serialize(Camera* v, Serializer* s)
    {
        SerializableField<Component>::Serialize(v, s);
        s->Serialize(v->projectionMatrix);
        s->Serialize(v->viewMatrix);
    }

    static void Deserialize(Camera* v, Serializer* s)
    {
        SerializableField<Component>::Deserialize(v, s);
        s->Deserialize(v->projectionMatrix);
        s->Deserialize(v->viewMatrix);
    }
};
} // namespace Engine
