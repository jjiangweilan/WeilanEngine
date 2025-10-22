#pragma once
#include "Core/Component/Component.hpp"

class OceanComponent;

namespace Game
{
struct BuoyancySample
{
    BuoyancySample(float3 samplePosition) : samplePosition(samplePosition),
                                            outWorldPosition(0, 0, 0) {}
    /**
     * @brief position relative to the GameObject
     */
    float3 samplePosition;

    float3 outWorldPosition;
};

class Boat : public Component
{
    DECLARE_COMPONENT_CONSTRUCT(Boat);
    DECLARE_SERIALIZATION();

    float initDistance = 2.0f;
    ObjPtr<OceanComponent> oceanComponent;
    std::vector<BuoyancySample> buoyancySamples;

public:
    void Tick() override;
    void IdleTick() override;
    void OnInit() override;

    // Boat
    void UpdateBoat(OceanComponent* ocean);
    void UpdateBuoyancy();
    float GetInitDistance() const { return initDistance; }
    void SetInitDistance(float v)
    {
        initDistance = v;
        buoyancySamples = {
            BuoyancySample(float3(-1.5f * initDistance, 0, 0)),
            BuoyancySample(float3(-0.5f * initDistance, 0, 0)),
            BuoyancySample(float3(0.5f * initDistance, 0, 0)),
            BuoyancySample(float3(1.5f * initDistance, 0, 0)),
        };
    }
    OceanComponent* GetOceanComponent();
    void SetOceanComponent(OceanComponent* o);
};
} // namespace Game
