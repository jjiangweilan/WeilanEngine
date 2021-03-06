#include "Component/Camera.hpp"

namespace KuangyeEngine
{
COMPONENT_DEFINATION(Component, Camera, 2);
Camera::Camera(Entity *entity) : Component(entity)
{
};

void Camera::SetProjectionMatrix(const float &fovy, const float &aspect, const float &zNear, const float &zFar)
{
    assert(0 && "You are probably setting a 2D camera");
}

void Camera::SetProjectionMatrix(const float &left, const float &right,
                                 const float &bottom, const float &top,
                                 const float &zNear, const float &zFar)
{
    assert(0 && "You are probaly settings a 3D camera");
}


void Camera::SetLayerMask(const int32_t& mask)
{
    m_layerMask = mask;
}

const int32_t &Camera::GetLayerMask() const
{
    return m_layerMask;
}

const int32_t &Camera::GetLayerMask()
{
    return m_layerMask;
}

} // namespace KuangyeEngine
