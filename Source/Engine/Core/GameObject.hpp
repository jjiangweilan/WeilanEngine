#pragma once

#include "Asset.hpp"
#include "Component/Component.hpp"
#include "EngineState.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <memory>
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

    std::unique_ptr<Asset> Clone() override;
    template <class T, class... Args>
    T* AddComponent(Args&&... args);

    void RemoveComponent(void* comp)
    {
        auto iter = std::find_if(components.begin(), components.end(), [comp](auto& p) { return p.get() == comp; });
        if (iter != components.end())
        {
            std::unique_ptr<Component>& comp = *iter;
            comp->Disable();
            components.erase(iter);
        }
    }

    template <class T>
    T* GetComponent();

    Component* GetComponent(const char* name); // obsolete

    std::vector<std::unique_ptr<Component>>& GetComponents();
    void SetScene(Scene* scene);
    Scene* GetScene();
    void Tick();
    void PrePhysicsTick();

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

    void SetLocalRotation(const glm::quat& rotation);
    void SetRotation(const glm::quat& rotation);
    void SetLocalPosition(const glm::vec3& localPosition);
    void SetPosition(const glm::vec3& position);
    void SetLocalScale(const glm::vec3& scale);
    void SetScale(const glm::vec3& scale);

    void Awake()
    {
        for (auto& c : components)
        {
            c->Awake();
        }
    }

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

        for (GameObject* child : children)
        {
            child->Translate(translate);
        }

        TransformChanged();
    }

    glm::vec3 GetPosition() const
    {
        return GetWorldMatrix()[3];
    }

    glm::vec3 GetLocalPosition() const
    {
        return position;
    }

    glm::vec3 GetScale() const
    {
        return scale;
    }

    glm::vec3 GetForward() const
    {
        return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[2]));
    }

    glm::vec3 GetUp() const
    {
        return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[1]));
    }

    glm::vec3 GetRight() const
    {
        return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[0]));
    }

    glm::vec3 GetEuluerAngles() const
    {
        return eulerAngles;
    }

    void SetEulerAngles(const glm::vec3& eulerAngles)
    {
        if (this->eulerAngles == eulerAngles)
            return;

        this->eulerAngles = eulerAngles;
        auto rotation = glm::quat(eulerAngles);
        SetLocalRotation(rotation);
    }

    glm::mat4 GetWorldMatrix() const;

    glm::quat GetRotation() const;

    void SetWorldMatrix(const glm::mat4& model);

    // this function should be used internally by Scene
    // it doesn't set the child's parent
    void RemoveChild(GameObject* child);
    void ResetTransform();

    void SetWantsToBeEnabled()
    {
        wantsToBeEnabled = true;
    }

    bool GetWantsTobeEnabledStateAndReset()
    {
        bool temp = wantsToBeEnabled;
        wantsToBeEnabled = false;
        return temp;
    }

    std::vector<std::unique_ptr<GameObject>>&& GetOwningChildren()
    {
        return std::move(owningChildren);
    }

    void OnLoadingFinished() override;

private:
    glm::vec3 position = glm::vec3(0);
    glm::vec3 scale = glm::vec3(1, 1, 1);
    glm::quat rotation = glm::quat(1, 0, 0, 0);
    // euler angle is defined as X * Y * Z (pitch yaw row), which coresponds to glm::quat(eulerAngles)
    glm::vec3 eulerAngles = glm::vec3(0, 0, 0);
    mutable glm::mat4 localMatrix;

    // when GameObject is being copied or deattached from a scene, it can't be enabled immediately
    // wantsToBeEnabled will be set to true whth enabled is false in that case
    bool enabled = false;
    bool wantsToBeEnabled = false;

    mutable bool updateLocalMatrix = true;

    std::vector<GameObject*> children;
    std::vector<std::unique_ptr<GameObject>> owningChildren;
    std::vector<std::unique_ptr<Component>> components;
    GameObject* parent = nullptr;
    Scene* gameScene = nullptr;

    inline bool EqualZero(const glm::vec3& v)
    {
        return glm::abs(v.x) < compareEpsilon && glm::abs(v.y) < compareEpsilon && glm::abs(v.z) < compareEpsilon;
    }

    void TransformChanged()
    {
        if (!EngineState::GetSingleton().isPlaying)
        {
            for (auto& c : components)
                c->TransformChanged();
        }
    }

    inline static const float compareEpsilon = 1e-6f;
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

    if (auto comp = GetComponent<T>())
    {
        results.push_back(comp);
    }

    for (auto c : children)
    {
        std::vector<T*> cs = c->GetComponentsInChildren<T>();
        results.insert(results.end(), cs.begin(), cs.end());
    }

    return results;
}
