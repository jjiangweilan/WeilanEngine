#pragma once

#include "Component.hpp"
#include "Engine/Library/Algorithms/Boids.hpp"

class [[LuaClass]] Boids : public Component
{
    DECLARE_COMPONENT(Boids);

public:
    void Tick() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;

    Algorithms::BoidsSettings& GetSettings() { return settings; }
    const Algorithms::BoidsSettings& GetSettings() const { return settings; }

private:
    Algorithms::BoidsSettings settings;
    std::vector<float3> velocities;
};
