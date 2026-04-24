#pragma once
#include "Engine/Runtime/Object/Component/Component.hpp"

/**
 * PlayerInteractable scene object
 * properties:
 *  1. Aimable
 *    Player can aim this object when aim button is pressed
 *  2. Moveable
 *    player can move this object when interaction button is pressed
 *  3. Stand On to Break
 */
// class PlayerInteractable : public Component
// {
//     DECLARE_OBJECT();
//
// public:
//     PlayerInteractable();
//     PlayerInteractable(GameObject* gameObject);
//     ~PlayerInteractable();
//
//     std::unique_ptr<Component> Clone(GameObject& owner) override;
//     const std::string& GetName() const override;
//     void Serialize(Serializer* s) const override;
//     void Deserialize(Serializer* s) override;
// };
