#include "Boat.hpp"
#include "Core/GameObject.hpp"
#include "Core/Time.hpp"
#include "Libs/Math.hpp"
#include "Modules/Ocean/OceanComponent.hpp"

#define GERSTNERWAVE_CPU_SIDE
namespace GPUResources
{
#include "Shaders/GerstnerWave.hlsl"
}

namespace Game
{
DEFINE_COMPONENT_CONSTRUCT(Boat, "76B5D7F8-997D-4662-A158-728D09FA5160") {}
DEFINE_SERIALIZATION(
    Boat,
    Component,
    SER(oceanComponent)
)

void Boat::Tick()
{
    if (oceanComponent)
    {
        UpdateBoat(oceanComponent);
    }
}

void Boat::UpdateBoat(OceanComponent* ocean)
{
    auto goPosition = gameObject->GetPosition();
    auto& waves = ocean->GetWaves();
    auto& globalTweak = ocean->GetGlobalTweak();
    for (auto& b : buoyancySamples)
    {
        float3 normal;
        float3 waveOffset;
        GPUResources::GerstenerWave(goPosition + b.samplePosition, waves.data(), waves.size(), ocean->areaScale, Time::TimeSinceLaunch(), normal, waveOffset);
    }
}

} // namespace Game
