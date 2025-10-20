#pragma once
#include "Core/Component/Component.hpp"

namespace Game
{
class Boat : public Component
{
    DECLARE_COMPONENT_CONSTRUCT(Boat);

public:
    void Tick() override;

    // Boat
    void UpdateBoat();
    void UpdateBuoyancy();
};
} // namespace Game
