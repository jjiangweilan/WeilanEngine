#pragma once
#include "Engine/Runtime/Object/Component/Component.hpp"

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

    // Imagine this is the boat you see on screen
    struct BoatShape
    {
        float3 centerPos; // this is the boat's centerPos

        // We use those two poses two simulate the boat's buoyance (tilt the shape)
        float3 frontPos;
        float3 backPos;
    } boatShape;

    float initDistance = 2.0f;
    ObjPtr<OceanComponent> oceanComponent;
    std::vector<BuoyancySample> buoyancySamples;

public:
    void Tick() override;
    void IdleTick() override;
    void OnAwake() override;
    void PrePhysicsTick() override;
    void DebugDraw() override;

    // Boat
    void UpdateBoat(OceanComponent* ocean);
    void UpdateBuoyancy();
    float GetInitDistance() const { return initDistance; }
    void SetInitDistance(float v)
    {
        initDistance = v;
        buoyancySamples = {
            BuoyancySample(float3(0, 0, -1.5f * initDistance)),
            BuoyancySample(float3(0, 0, -0.5f * initDistance)),
            BuoyancySample(float3(0, 0, 0.5f * initDistance)),
            BuoyancySample(float3(0, 0, 1.5f * initDistance)),
        };
    }
    OceanComponent* GetOceanComponent();
    void SetOceanComponent(OceanComponent* o);
};
} // namespace Game
