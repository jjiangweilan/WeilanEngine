#pragma once
#include "Core/Component/Component.hpp"
#include "Core/Math/Geometry.hpp"

class ReflectionProbe : public Component
{
    DECLARE_OBJECT();

public:
    ReflectionProbe();
    ReflectionProbe(GameObject* gameObject);
    ~ReflectionProbe();
    const std::string& GetName() override;

    std::unique_ptr<Component> Clone(GameObject& owner) override { return nullptr; }
    std::span<Frustum> GetViewProjectionMatrices();
    // void Serialize(Serializer* s) const override;
    // void Deserialize(Serializer* s) override;
    void OnEnable() override;
    void OnDisable() override;

private:
    Frustum frustums[6];

    void TransformChanged() override;
};
