#pragma once

#include "Component.hpp"
#include "Engine/Runtime/System/Navigation/NavSystem.hpp"

class Navigation : public Component
{
    DECLARE_COMPONENT(Navigation);

public:
    void DebugDraw() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;

    void SetNavData(ObjPtr<NavData> data);
    ObjPtr<NavData> GetNavData() const { return navData; }
    NavSystem& GetNavSystem() { return navSystem; }
    const NavSystem& GetNavSystem() const { return navSystem; }

private:
    ObjPtr<NavData> navData;
    NavSystem navSystem;
};
