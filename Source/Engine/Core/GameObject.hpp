#pragma once

#include "Asset.hpp"
#include "Component/Component.hpp"
#include "Libs/Ptr.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

class Scene;

enum class RotationCoordinate
{
    Self,
    Parent,
    World,
};

class GameObject : public Asset
{
    DECLARE_ASSET();

public:
    GameObject(Scene* gameScene);
    GameObject(GameObject&& other);
    GameObject(const GameObject& other);
    GameObject();
    ~GameObject();
    template <class T, class... Args>
    T* AddComponent(Args&&... args);

    template <class T>
    T* GetComponent();

    Component* GetComponent(const char* name); // obsolete

    std::vector<std::unique_ptr<Component>>& GetComponents();
    void SetScene(Scene* scene);
    Scene* GetScene();
    void Tick();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    const std::vector<GameObject*>& GetChildren()
    {
        return children;
    }

    template <class T>
    std::vector<T*> GetComponentsInChildren();

    bool IsEnabled()
    {
        return enabled;
    }

    void SetEnable(bool isEnabled);

    GameObject* GetParent()
    {
        return parent;
    }
    void SetParent(GameObject* parent);

    void SetRotation(const glm::quat& rotation)
    {
        updateModelMatrix = true;
        this->rotation = rotation;
    }
    void SetPosition(const glm::vec3& position)
    {
        updateModelMatrix = true;
        auto delta = position - this->position;
        this->position = position;

        for (GameObject* child : children)
        {
            child->Translate(delta);
        }
    };

    void SetScale(const glm::vec3& scale)
    {
        updateModelMatrix = true;
        this->scale = scale;
    }

    void Awake()
    {
        for (auto& c : components)
        {
            c->Awake();
        }
    }

    void Rotate(float angle, glm::vec3 axis, RotationCoordinate coord)
    {
        updateModelMatrix = true;
        if (coord == RotationCoordinate::Self)
        {
            rotation = glm::rotate(rotation, angle, axis);
        }
        else if (coord == RotationCoordinate::Parent && parent != nullptr)
        {}
        else if (coord == RotationCoordinate::World)
        {
            // rotate around world
            glm::mat4 trs = glm::rotate(glm::mat4(1), angle, axis) * GetModelMatrix();

            SetModelMatrix(trs);
        }
    }
    void Translate(const glm::vec3& translate)
    {
        updateModelMatrix = true;
        this->position += translate;

        for (GameObject* child : children)
        {
            child->Translate(translate);
        }
    }

    glm::vec3 GetPosition() const
    {
        return position;
    }

    glm::vec3 GetScale() const
    {
        return scale;
    };

    glm::quat GetRotation() const
    {
        return rotation;
    };

    glm::vec3 GetForward() const
    {
        return glm::mat4_cast(rotation)[2];
    }

    glm::vec3 GetEuluerAngles() const
    {
        return eulerAngles;
    }

    void SetEulerAngles(const glm::vec3& eulerAngles)
    {
        updateModelMatrix = true;
        this->eulerAngles = eulerAngles;
        rotation = glm::quat(eulerAngles);
    }

    glm::mat4 GetModelMatrix()
    {
        if (updateModelMatrix)
        {
            modelMatrix =
                glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
            updateModelMatrix = false;
        }

        return modelMatrix;
    }
    void SetModelMatrix(const glm::mat4& model);

    // this function should be used internally by Scene
    // it doesn't set the child's parent
    void RemoveChild(GameObject* child);
    void ResetTransform();

private:
    glm::vec3 position = glm::vec3(0);
    glm::vec3 scale = glm::vec3(1, 1, 1);

    glm::quat rotation = glm::quat(1, 0, 0, 0);
    // euler angle is defined as X * Y * Z (pitch yaw row), which coresponds to glm::quat(eulerAngles)
    glm::vec3 eulerAngles = glm::vec3(0, 0, 0);
    glm::mat4 modelMatrix;
    bool enabled = false;
    bool updateModelMatrix = true;

    std::vector<GameObject*> children;
    std::vector<std::unique_ptr<Component>> components;
    GameObject* parent = nullptr;
    Scene* gameScene = nullptr;
};

template <class T, class... Args>
T* GameObject::AddComponent(Args&&... args)
{
    auto p = std::make_unique<T>(this, args...);
    T* temp = p.get();
    components.push_back(std::move(p));
    temp->Enable();
    return temp;
}

template <class T>
T* GameObject::GetComponent()
{
    for (auto& p : components)
    {
        T* cast = dynamic_cast<T*>(p.get());
        if (cast != nullptr)
            return cast;
    }
    return nullptr;
}

template <class T>
std::vector<T*> GameObject::GetComponentsInChildren()
{
    std::vector<T*> results;

    results.push_back(GetComponent<T>());

    for (auto c : children)
    {
        std::vector<T*> cs = c->GetComponentsInChildren<T>();
        results.insert(results.end(), cs.begin(), cs.end());
    }

    return results;
}
