#pragma once
#include "Component.hpp"
#include "Rendering/SHProbe.hpp"

class Texture;
class SceneEnvironment : public Component
{
    DECLARE_OBJECT();

public:
    SceneEnvironment() : Component(nullptr) {};
    SceneEnvironment(GameObject* owner) : Component(owner) {};
    ~SceneEnvironment() override {};

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() const override;
    std::unique_ptr<Component> Clone(GameObject& owner) override
    {
        auto clone = std::make_unique<SceneEnvironment>();
        return clone;
    }

    void UpdateSkyboxProbe();
    const std::vector<float4>& GetSkyboxProbeCoefficients() const
    {
        if (skyboxProbe.HasSH())
            return skyboxProbe.GetSHCoefficients();
        else
        {
            static std::vector<float4> zeros(9);
            return zeros;
        }
    }

    void OnEnable() override;
    void OnDisable() override;

    void DebugDrawSkyboxProbe(const float3& position);

private:
    SHProbe skyboxProbe;
};
