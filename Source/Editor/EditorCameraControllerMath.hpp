#pragma once

#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace Editor
{
struct EditorCameraAngles
{
    float yaw = 0.0f;
    float pitch = 0.0f;
};

inline EditorCameraAngles ComputeEditorCameraAnglesFromForward(const glm::vec3& forward, float fallbackYaw = 0.0f)
{
    if (glm::length2(forward) <= 1e-6f)
    {
        return {fallbackYaw, 0.0f};
    }

    glm::vec3 normalizedForward = glm::normalize(forward);
    EditorCameraAngles angles;
    angles.pitch = glm::asin(glm::clamp(normalizedForward.y, -1.0f, 1.0f));

    glm::vec2 flatForward = glm::vec2(-normalizedForward.x, -normalizedForward.z);
    if (glm::length2(flatForward) <= 1e-6f)
    {
        angles.yaw = fallbackYaw;
    }
    else
    {
        angles.yaw = glm::atan(flatForward.x, flatForward.y);
    }

    return angles;
}

inline glm::quat BuildEditorCameraRotation(float yaw, float pitch)
{
    glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat pitchQuat = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    return glm::normalize(yawQuat * pitchQuat);
}
} // namespace Editor
