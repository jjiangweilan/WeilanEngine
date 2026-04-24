#pragma
#include "Boat.hpp"
#include "Engine/Runtime/Object/Component/Component.hpp"

namespace Game
{
class GameGlobal : public Component
{
    DECLARE_COMPONENT_CONSTRUCT(GameGlobal);
    DECLARE_SERIALIZATION();

public:
    ObjPtr<Boat> boat;

public:
    void OnInit() override;
    void Tick() override;

private:
    void UpdateBoat();
    void BoatUpdateBuoyancy();
};

} // namespace Game
