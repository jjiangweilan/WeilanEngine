#pragma once
#include "Core/Component/Component.hpp"

class OceanComponent;

namespace Game
{
struct BuoyancySample
{
    /**
     * @brief position relative to the GameObject
     */
    float3 samplePosition;

    float3 worldPosition;
};

class Boat : public Component
{
    DECLARE_COMPONENT_CONSTRUCT(Boat);
    DECLARE_SERIALIZATION();

    ObjPtr<OceanComponent> oceanComponent;
    std::vector<BuoyancySample> buoyancySamples;

public:
    void Tick() override;

    // Boat
    void UpdateBoat(OceanComponent* ocean);
    void UpdateBuoyancy();
};
} // namespace Game
