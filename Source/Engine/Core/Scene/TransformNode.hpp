#pragma once
#include "Core/Ptr.hpp"
#include "Libs/EnumFlags.hpp"

enum class TransformFlag : uint32_t
{
    None = 0,
    DontChangeHierarchy
};

class GameObject;
enum class RotationCoordinate
{
    Self,
    Parent,
    World,
};

class TransformNode
{
    DECLARE_SERIALIZABLE()

    TransformNode* parent;
    std::vector<TransformNode*> children;
    ObjPtr<GameObject> gameObject;

    glm::vec3 position = glm::vec3(0);
    glm::vec3 scale = glm::vec3(1, 1, 1);
    glm::quat rotation = glm::quat(1, 0, 0, 0);
    glm::vec3 eulerAngles = glm::vec3(0, 0, 0); // euler angle is defined as X * Y * Z (pitch yaw row), which coresponds to glm::quat(eulerAngles)

    mutable glm::mat4 localMatrix;
    mutable glm::mat4 worldMatrix;
    mutable bool transformChanged = true;
    mutable bool updateLocalMatrix = true;
    TransformFlag flags = TransformFlag::None;

    bool valid = false;

public:
    void SetParent(TransformNode* parent, bool keepWorldSpacePostion = true);
    void RemoveChild(TransformNode* node);
    std::span<TransformNode> GetChildren() const;

    void SetLocalRotation(const glm::quat& rotation);
    void SetRotation(const glm::quat& rotation);
    void SetLocalPosition(const glm::vec3& localPosition);
    void SetPosition(const glm::vec3& position);
    void SetLocalScale(const glm::vec3& scale);
    void SetScale(const glm::vec3& scale);

    void LookAt(const glm::vec3& to)
    {
        if (glm::length(to) < compareEpsilon)
            return;

        glm::vec3 forward = glm::normalize(to);
        glm::vec3 up = glm::vec3(0, 1, 0);

        // Handle case where forward is parallel to up vector
        if (glm::abs(glm::dot(forward, up)) > 0.99f)
        {
            up = glm::vec3(1, 0, 0);
        }

        glm::vec3 right = glm::normalize(glm::cross(forward, up));
        up = glm::cross(right, forward);

        glm::mat3 rotationMatrix = glm::mat3(right, up, -forward);
        glm::quat newRotation = glm::quat_cast(rotationMatrix);

        SetRotation(newRotation);
    }

    void Rotate(glm::quat quaternion) { SetLocalRotation(quaternion * rotation); }

    void Rotate(float angle, glm::vec3 axis, RotationCoordinate coord)
    {
        updateLocalMatrix = true;
        if (coord == RotationCoordinate::Self)
        {
            rotation = glm::rotate(rotation, angle, axis);
        }
        else if (coord == RotationCoordinate::Parent && parent != nullptr)
        {}
        else if (coord == RotationCoordinate::World)
        {
            // rotate around world
            glm::mat4 trs = glm::rotate(glm::mat4(1), angle, axis) * GetWorldMatrix();

            SetWorldMatrix(trs);
        }

        TransformChanged();
    }

    void RotateAround(const glm::vec3& point, const glm::vec3& axis, float angle)
    {
        glm::mat4 trs = glm::translate(glm::mat4(1), point) * glm::rotate(glm::mat4(1), angle, axis) *
                        glm::translate(glm::mat4(1), -point) * GetWorldMatrix();
        SetWorldMatrix(trs);
    }

    void Translate(const glm::vec3& translate)
    {
        if (translate == glm::vec3{0, 0, 0})
            return;

        updateLocalMatrix = true;
        this->position += translate;

        for (TransformNode& child : children)
        {
            child.Translate(translate);
        }

        TransformChanged();
    }

    glm::vec3 GetPosition() const { return GetWorldMatrix()[3]; }
    glm::vec3 GetLocalPosition() const { return position; }
    glm::vec3 GetScale() const
    {
        auto m = GetWorldMatrix();
        return {glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2]))};
    }
    glm::vec3 GetLocalScale() const { return scale; }
    glm::quat GetLocalRotation() const { return rotation; }
    glm::vec3 GetForward() const { return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[2])); }
    glm::vec3 GetUp() const { return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[1])); }
    glm::vec3 GetRight() const { return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[0])); }
    glm::vec3 GetEuluerAngles() const { return eulerAngles; }

    void SetEulerAngles(const glm::vec3& eulerAngles)
    {
        if (this->eulerAngles == eulerAngles)
            return;

        this->eulerAngles = eulerAngles;
        auto rotation = glm::quat(eulerAngles);

        // set local rotation
        this->rotation = rotation;
        updateLocalMatrix = true;

        TransformChanged();
    }

    glm::mat4 GetWorldMatrix() const;
    const glm::mat4& GetLocalMatrix() const;

    glm::quat GetRotation() const;

    void SetWorldMatrix(const glm::mat4& model);

private:
    inline bool EqualZero(const glm::vec3& v)
    {
        return glm::abs(v.x) < compareEpsilon && glm::abs(v.y) < compareEpsilon && glm::abs(v.z) < compareEpsilon;
    }
    inline static const float compareEpsilon = 1e-6f;
    void TransformChanged();
};
