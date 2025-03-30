#pragma once
#include "Core/Object.hpp"
#include "Rendering/SHProbe.hpp"

class SceneSettings : public Component
{
    DECLARE_OBJECT();

public:
    SceneSettings() : Component(nullptr) {};
    SceneSettings(GameObject* gameObject) : Component(gameObject) {}

    void UpdateSkyboxProbe();
    void DebugDrawSkyboxProbe(const float3& position);

    const std::string& GetName() override;

private:
    SHProbe skyboxProbe;
};
