#include "GameGlobal.hpp"
#include "Engine/Library/TypeReflection.hpp"

namespace Game
{
DEFINE_COMPONENT_CONSTRUCT(GameGlobal, "B884BA22-4E19-4F9A-A171-3BACA6C38B91")
{
}

TYPE_REFLECTION_MEMBER_VARIABLES(
    GameGlobal,
    TYPE_REFLECTION_MEM(GameGlobal, boat)
);

DEFINE_SERIALIZATION(
    GameGlobal,
    Component,
    SER(boat)
);

void GameGlobal::OnAwake()
{
}

void GameGlobal::Tick()
{
    UpdateBoat();
}

void GameGlobal::UpdateBoat()
{
    BoatUpdateBuoyancy();
}

void GameGlobal::BoatUpdateBuoyancy()
{
}
} // namespace Game
