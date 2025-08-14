#include "../EditorState.hpp"
#include "Core/Component/GameScript.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Core/Scene/PhysicsScene.hpp"
#include "Inspector.hpp"
#include "Jolt/Physics/Body/Body.h"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{
class PhysicsBodyInspector : public Inspector<PhysicsBody>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        /** Sensor **/
        bool isSensor = target->IsSensor();
        if (GUI::Checkbox("Is Sensor", &isSensor))
        {
            target->SetSensor(isSensor);
        }

        const char* items[] = {"Scene", "Moving", "Interactable"}; // defined in PhysicsLayer.hpp
        int currentItenIndex = static_cast<int>(target->GetLayer());
        if (GUI::ComboLabeled("Layer", &currentItenIndex, items, IM_ARRAYSIZE(items)))
        {
            if (currentItenIndex == 0)
                target->SetLayer(PhysicsLayer::Scene);
            else if (currentItenIndex == 1)
                target->SetLayer(PhysicsLayer::Moving);
            else if (currentItenIndex)
                target->SetLayer(PhysicsLayer::Interactable);
        }

        /** Shape **/
        const char* shapes[] = {"Box", "Sphere", "Mesh", "Capsule", "Compound"};
        int currentShapeIndex = static_cast<int>(target->GetShape());
        glm::vec4 bodyScale = target->GetBodyScale();
        if (GUI::ComboLabeled("Shape", &currentShapeIndex, shapes, IM_ARRAYSIZE(shapes)))
        {
            if (currentShapeIndex == 0)
                target->SetShape(PhysicsBodyShapes::Box);
            else if (currentShapeIndex == 1)
                target->SetShape(PhysicsBodyShapes::Sphere);
            else if (currentShapeIndex == 2)
                target->SetShape(PhysicsBodyShapes::Mesh);
            else if (currentShapeIndex == 3)
                target->SetShape(PhysicsBodyShapes::Capsule);
            else if (currentItenIndex == 4)
                target->SetShape(PhysicsBodyShapes::Compound);
        }

        /** Position **/
        auto bodyOffset = target->GetBodyOffset();
        if (GUI::DragFloat3("Body Offset", &bodyOffset[0]))
        {
            target->SetBodyOffset(bodyOffset);
        }

        /** Scale **/
        if (currentShapeIndex == 0)
        {
            if (GUI::DragFloat3("Box Scale", &bodyScale[0]))
                target->SetBodyScale(bodyScale);
        }
        else if (currentShapeIndex == 1)
        {
            if (GUI::DragFloat("Sphere Scale", &bodyScale[0]))
            {
                target->SetBodyScale(bodyScale);
            }
        }
        else if (currentShapeIndex == 3)
        {
            if (GUI::DragFloat("Capsule Height", &bodyScale[0]))
            {
                target->SetBodyScale(bodyScale);
            }

            if (GUI::DragFloat("Capsule Radius", &bodyScale[1]))
            {
                target->SetBodyScale(bodyScale);
            }
        }

        // gravity factor
        float gf = target->GetGravityFactory();
        if (GUI::DragFloat("Gravity Factor", &gf))
        {
            target->SetGravityFactor(gf);
        }

        // motion type
        JPH::EMotionType motionType = target->GetMotionType();
        int currentMotionType = static_cast<int>(motionType);
        const char* motionTypes[] = {"Static", "Kinematic", "Dynamic"};
        if (GUI::ComboLabeled("Motion Type", &currentMotionType, motionTypes, IM_ARRAYSIZE(motionTypes)))
        {
            target->SetMotionType(static_cast<JPH::EMotionType>(currentMotionType));
        }

        if (motionType == JPH::EMotionType::Kinematic)
        {
            bool shouldKinematicGenerateContactPointsWithNonDynamic =
                target->ShouldKinematicGenerateContactPointsWithNonDynamic();
            if (GUI::Checkbox("Collide With NonDynamic", &shouldKinematicGenerateContactPointsWithNonDynamic))
            {
                target->SetKinematicCollideWithNonDynamic(shouldKinematicGenerateContactPointsWithNonDynamic);
            }
        }

        target->debugDrawRequest = true;

        ImGui::SeparatorText("Lua Callback");
        ImGuiTreeNodeFlags callbackTreeNodeFlags = 0;

        auto contactAddedLuaCallbacks = target->GetContactAddedLuaCallbacks();
        if (ImGui::TreeNodeEx("Contact Added", contactAddedLuaCallbacks.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawPhysicsLuaCallbackInspector(contactAddedLuaCallbacks))
            {
                target->SetContactAddedLuaCallbacks(contactAddedLuaCallbacks);
            }
            ImGui::TreePop();
        }

        auto contactRemovedLuaCallbacks = target->GetContactRemovedLuaCallbacks();
        if (ImGui::TreeNodeEx(
                "Contact Removed",
                contactRemovedLuaCallbacks.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            if (DrawPhysicsLuaCallbackInspector(contactRemovedLuaCallbacks))
            {
                target->SetContactRemovedLuaCallbacks(contactRemovedLuaCallbacks);
            }
            ImGui::TreePop();
        }

        auto contactPersistedLuaCallbacks = target->GetContactPersistedLuaCallbacks();
        if (ImGui::TreeNodeEx(
                "Contact Persisted",
                contactPersistedLuaCallbacks.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen
            ))
        {
            if (DrawPhysicsLuaCallbackInspector(contactPersistedLuaCallbacks))
            {
                target->SetContactPersistedLuaCallbacks(contactPersistedLuaCallbacks);
            }
            ImGui::TreePop();
        }

        GUI::SeparatorTextLabeled("Status");
        auto body = target->GetBody();
        if (body)
        {
            GUI::TextFormatted("ID", "%i", body->GetID().GetIndexAndSequenceNumber());
            GUI::Text("Body Type", body->GetBodyType() == JPH::EBodyType::RigidBody ? "Rigid" : "Soft");
            GUI::Text("Is Active", body->IsActive() ? "True" : "False");
            GUI::Text("Motion Type", motionTypes[currentMotionType]);
            GUI::Text("Is Sensor", body->IsSensor() ? "True" : "False");
            GUI::Text("Can Be Kinematic or Dynamic", body->CanBeKinematicOrDynamic() ? "True" : "False");
            GUI::Text("Use Manifold Reduction", body->GetUseManifoldReduction() ? "True" : "False");
            GUI::Text("Broad Phase Layer", BPLayerInterfaceImpl::GetBroadPhaseLayerNameImpl(body->GetBroadPhaseLayer()));
            GUI::TextFormatted("Object Layer", "%d", body->GetObjectLayer());
            GUI::TextFormatted("Friction", "%.2f", body->GetFriction());
            GUI::TextFormatted("Restitution", "%.2f", body->GetRestitution());
            GUI::TextFormatted("User Data", "%llu", body->GetUserData());

            if (!body->IsStatic() && body->GetMotionProperties())
            {
                GUI::Text("Allow Sleeping", body->GetAllowSleeping() ? "True" : "False");
                GUI::TextFormatted("Linear Velocity", "(%.2f, %.2f, %.2f)",
                    body->GetLinearVelocity().GetX(),
                    body->GetLinearVelocity().GetY(),
                    body->GetLinearVelocity().GetZ()
                );
                GUI::TextFormatted("Angular Velocity", "(%.2f, %.2f, %.2f)",
                    body->GetAngularVelocity().GetX(),
                    body->GetAngularVelocity().GetY(),
                    body->GetAngularVelocity().GetZ()
                );
                if (body->IsDynamic())
                {
                    GUI::TextFormatted("Accumulated Force", "(%.2f, %.2f, %.2f)",
                        body->GetAccumulatedForce().GetX(),
                        body->GetAccumulatedForce().GetY(),
                        body->GetAccumulatedForce().GetZ()
                    );

                    GUI::TextFormatted("Accumulated Torque", "(%.2f, %.2f, %.2f)",
                        body->GetAccumulatedTorque().GetX(),
                        body->GetAccumulatedTorque().GetY(),
                        body->GetAccumulatedTorque().GetZ()
                    );
                }
                GUI::Text("In Broad Phase", body->IsInBroadPhase() ? "True" : "False");
                GUI::Text("Collision Cache Invalid", body->IsCollisionCacheInvalid() ? "True" : "False");
                GUI::Text("Shape", body->GetShape() ? "Valid" : "Null");
                GUI::TextFormatted("Position", "(%.2f, %.2f, %.2f)",
                    body->GetPosition().GetX(),
                    body->GetPosition().GetY(),
                    body->GetPosition().GetZ()
                );
                GUI::TextFormatted("Rotation", "(%.2f, %.2f, %.2f, %.2f)",
                    body->GetRotation().GetX(),
                    body->GetRotation().GetY(),
                    body->GetRotation().GetZ(),
                    body->GetRotation().GetW()
                );
                GUI::TextFormatted("Center of Mass Position", "(%.2f, %.2f, %.2f)",
                    body->GetCenterOfMassPosition().GetX(),
                    body->GetCenterOfMassPosition().GetY(),
                    body->GetCenterOfMassPosition().GetZ()
                );
                GUI::TextFormatted("World Space Bounds", "(%.2f, %.2f, %.2f) - (%.2f, %.2f, %.2f)",
                    body->GetWorldSpaceBounds().mMin.GetX(),
                    body->GetWorldSpaceBounds().mMin.GetY(),
                    body->GetWorldSpaceBounds().mMin.GetZ(),
                    body->GetWorldSpaceBounds().mMax.GetX(),
                    body->GetWorldSpaceBounds().mMax.GetY(),
                    body->GetWorldSpaceBounds().mMax.GetZ()
                );
            }
        }
    }

private:
    static const char _register;

    bool DrawPhysicsLuaCallbackInspector(DynamicArray<PhysicsLuaCallback>& callbacks)
    {
        bool changed = false;
        for (size_t i = 0; i < callbacks.size(); ++i)
        {
            bool wantRemove = false;
            ImGui::PushID(static_cast<int>(i));
            GUI::TextFormatted("Index", "%zu", i + 1);
            ImGui::SameLine();
            if (GUI::ButtonSimple("x"))
            {
                wantRemove = true;
            }
            ImGui::SameLine();
            GameScript* gameScript = callbacks[i].gameScript;
            if (GUI::ObjectField("", gameScript))
            {
                changed = true;
                callbacks[i].gameScript = gameScript;
            }
            ImGui::SameLine();
            if (GUI::InputText("##Callback", callbacks[i].callback, "Function Name"))
            {
                changed = true;
            }
            if (wantRemove)
            {
                changed = true;
                callbacks.erase(callbacks.begin() + i);
                --i; // Adjust index after removal
            }
            ImGui::PopID();
        }

        if (GUI::ButtonSimple("Add Callback"))
        {
            changed = true;
            PhysicsLuaCallback cb;
            cb.gameScript = nullptr;
            cb.callback = "";
            callbacks.push_back(cb);
        }

        return changed;
    }
};

const char PhysicsBodyInspector::_register = InspectorRegistry::Register<PhysicsBodyInspector, PhysicsBody>();

} // namespace Editor
