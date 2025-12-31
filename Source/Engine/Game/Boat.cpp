#include "Boat.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/Module/Ocean/OceanComponent.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Library/TypeReflection.hpp"

#define GERSTNERWAVE_CPU_SIDE
namespace GPUResources
{
#include "Engine/Shaders/GerstnerWave.hlsl"
}

namespace Game
{
DEFINE_COMPONENT_CONSTRUCT(Boat, "76B5D7F8-997D-4662-A158-728D09FA5160")
{
}

TYPE_REFLECTION_MEMBER_VARIABLES(
    Game::Boat,
    TYPE_REFLECTION_MEM(Game::Boat, oceanComponent),
    TYPE_REFLECTION_MEM(Game::Boat, initDistance)
);

DEFINE_SERIALIZATION(
    Boat,
    Component,
    SER(oceanComponent),
    SER(initDistance)
)

void Boat::OnInit()
{
    SetInitDistance(initDistance);
}

void Boat::IdleTick()
{
    Tick();
}

void Boat::Tick()
{
    if (oceanComponent)
    {
        UpdateBoat(oceanComponent);
    }
}

void Boat::UpdateBoat(OceanComponent* ocean)
{
    ENGINE_SCOPED_PROFILE("Boat UpdateBoat");
    auto goPosition = gameObject->GetPosition();
    auto& waves = ocean->GetGPUWaveCache();
    auto& globalTweak = ocean->GetGlobalTweak();
    for (auto& b : buoyancySamples)
    {
        float3 normal;
        float3 waveOffset;
        GPUResources::GerstnerWave(goPosition + b.samplePosition, waves.data(), waves.size(), ocean->GetAreaScale(), Time::TimeSinceLaunch(), normal, waveOffset);

        b.outWorldPosition = b.samplePosition + goPosition + float3(0, waveOffset.y, 0);
    }
}

OceanComponent* Boat::GetOceanComponent()
{
    return oceanComponent.Get();
}

void Boat::SetOceanComponent(OceanComponent* o)
{
    oceanComponent = o;
}

void Boat::PrePhysicsTick()
{
}

void Boat::DebugDraw()
{
    Graphics::DrawSphere(gameObject->GetPosition(), float3(0.2f));
}

} // namespace Game
  //
