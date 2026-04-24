#pragma once
#include "Engine/Core/Object.hpp"
#include "Engine/Runtime/System/Rendering/SHProbe.hpp"

class SceneSettings : public Component
{
    DECLARE_OBJECT();

public:
    SceneSettings() : Component(nullptr) {};
    SceneSettings(GameObject* gameObject) : Component(gameObject) {}

    void UpdateSkyboxProbe();
    void DebugDrawSkyboxProbe(const float3& position);

    const std::string& GetName() const override;

private:
    SHProbe skyboxProbe;
};
