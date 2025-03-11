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
        if (ImGui::Checkbox("isSensor", &isSensor))
        {
            target->SetSensor(isSensor);
        }

        const char* items[] = {"Scene", "Moving", "Interactable"}; // defined in PhysicsLayer.hpp
        int currentItenIndex = static_cast<int>(target->GetLayer());
        if (ImGui::Combo("combo", &currentItenIndex, items, IM_ARRAYSIZE(items)))
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
        if (ImGui::Combo("Shape", &currentShapeIndex, shapes, IM_ARRAYSIZE(shapes)))
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
        if (ImGui::DragFloat3("Body Offset", &bodyOffset[0]))
        {
            target->SetBodyOffset(bodyOffset);
        }

        /** Scale **/
        if (currentShapeIndex == 0)
        {
            if (ImGui::DragFloat3("Box Scale", &bodyScale[0]))
                target->SetBodyScale(bodyScale);
        }
        else if (currentShapeIndex == 1)
        {
            if (ImGui::DragFloat("Sphere Scale", &bodyScale[0]))
            {
                target->SetBodyScale(bodyScale);
            }
        }
        else if (currentShapeIndex == 3)
        {
            if (ImGui::DragFloat("Capsule Height", &bodyScale[0]))
            {
                target->SetBodyScale(bodyScale);
            }

            if (ImGui::DragFloat("Capsule Radius", &bodyScale[1]))
            {
                target->SetBodyScale(bodyScale);
            }
        }

        // gravity factor
        float gf = target->GetGravityFactory();
        if (ImGui::InputFloat("gravity factor", &gf))
        {
            target->SetGravityFactor(gf);
        }

        // motion type
        JPH::EMotionType motionType = target->GetMotionType();
        int currentMotionType = static_cast<int>(motionType);
        const char* motionTypes[] = {"Static", "Kinematic", "Dynamic"};
        if (ImGui::Combo("Motion Type", &currentMotionType, motionTypes, IM_ARRAYSIZE(motionTypes)))
        {
            target->SetMotionType(static_cast<JPH::EMotionType>(currentMotionType));
        }

        if (motionType == JPH::EMotionType::Kinematic)
        {
            bool shouldKinematicGenerateContactPointsWithNonDynamic =
                target->ShouldKinematicGenerateContactPointsWithNonDynamic();
            if (ImGui::Checkbox("Collide With NonDynamic", &shouldKinematicGenerateContactPointsWithNonDynamic))
            {
                target->SetKinematicCollideWithNonDynamic(shouldKinematicGenerateContactPointsWithNonDynamic);
            }
        }

        target->debugDrawRequest = true;

        ImGui::SeparatorText("Lua Callback");
        ImGui::Indent(5);
        ImGui::SeparatorText("Contact Added");
        auto contactAddedLuaCallbacks = target->GetContactAddedLuaCallbacks();
        if (DrawPhysicsLuaCallbackInspector(contactAddedLuaCallbacks))
        {
            target->SetContactAddedLuaCallbacks(contactAddedLuaCallbacks);
        }
        ImGui::SeparatorText("Contact Removed");
        auto contactRemovedLuaCallbacks = target->GetContactRemovedLuaCallbacks();
        if (DrawPhysicsLuaCallbackInspector(contactRemovedLuaCallbacks))
        {
            target->SetContactRemovedLuaCallbacks(contactRemovedLuaCallbacks);
        }
        ImGui::SeparatorText("Contact Persisted");
        auto contactPersistedLuaCallbacks = target->GetContactPersistedLuaCallbacks();
        if (DrawPhysicsLuaCallbackInspector(contactPersistedLuaCallbacks))
        {
            target->SetContactPersistedLuaCallbacks(contactPersistedLuaCallbacks);
        }
        ImGui::Indent(-5);

        ImGui::SeparatorText("Status");
        auto& body = *target->GetBody();
        ImGui::LabelText("ID", "%i", body.GetID().GetIndexAndSequenceNumber());
        ImGui::LabelText("Body Type", "%s", body.GetBodyType() == JPH::EBodyType::RigidBody ? "Rigid" : "Soft");
        ImGui::LabelText("Is Active", "%s", body.IsActive() ? "True" : "False");

        ImGui::LabelText("Motion Type", "%s", motionTypes[currentMotionType]);
        ImGui::LabelText("Is Sensor", "%s", body.IsSensor() ? "True" : "False");
        ImGui::LabelText("Can Be Kinematic or Dynamic", "%s", body.CanBeKinematicOrDynamic() ? "True" : "False");
        ImGui::LabelText("Use Manifold Reduction", "%s", body.GetUseManifoldReduction() ? "True" : "False");
        // ImGui::LabelText("Sensor Detects Static", "%s", body.SensorDetectsStatic() ? "True" : "False");
        ImGui::LabelText(
            "Broad Phase Layer",
            "%s",
            BPLayerInterfaceImpl::GetBroadPhaseLayerNameImpl(body.GetBroadPhaseLayer())
        );
        ImGui::LabelText("Object Layer", "%d", body.GetObjectLayer());
        ImGui::LabelText("Friction", "%.2f", body.GetFriction());
        ImGui::LabelText("Restitution", "%.2f", body.GetRestitution());
        ImGui::LabelText("User Data", "%llu", body.GetUserData());
        if (body.GetMotionProperties())
        {
            ImGui::LabelText("Allow Sleeping", "%s", body.GetAllowSleeping() ? "True" : "False");
            ImGui::LabelText(
                "Linear Velocity",
                "(%.2f, %.2f, %.2f)",
                body.GetLinearVelocity().GetX(),
                body.GetLinearVelocity().GetY(),
                body.GetLinearVelocity().GetZ()
            );
            ImGui::LabelText(
                "Angular Velocity",
                "(%.2f, %.2f, %.2f)",
                body.GetAngularVelocity().GetX(),
                body.GetAngularVelocity().GetY(),
                body.GetAngularVelocity().GetZ()
            );
            ImGui::LabelText(
                "Accumulated Force",
                "(%.2f, %.2f, %.2f)",
                body.GetAccumulatedForce().GetX(),
                body.GetAccumulatedForce().GetY(),
                body.GetAccumulatedForce().GetZ()
            );
            ImGui::LabelText(
                "Accumulated Torque",
                "(%.2f, %.2f, %.2f)",
                body.GetAccumulatedTorque().GetX(),
                body.GetAccumulatedTorque().GetY(),
                body.GetAccumulatedTorque().GetZ()
            );
            ImGui::LabelText("In Broad Phase", "%s", body.IsInBroadPhase() ? "True" : "False");
            ImGui::LabelText("Collision Cache Invalid", "%s", body.IsCollisionCacheInvalid() ? "True" : "False");
            ImGui::LabelText("Shape", "%s", body.GetShape() ? "Valid" : "Null");
            ImGui::LabelText(
                "Position",
                "(%.2f, %.2f, %.2f)",
                body.GetPosition().GetX(),
                body.GetPosition().GetY(),
                body.GetPosition().GetZ()
            );
            ImGui::LabelText(
                "Rotation",
                "(%.2f, %.2f, %.2f, %.2f)",
                body.GetRotation().GetX(),
                body.GetRotation().GetY(),
                body.GetRotation().GetZ(),
                body.GetRotation().GetW()
            );
            ImGui::LabelText(
                "Center of Mass Position",
                "(%.2f, %.2f, %.2f)",
                body.GetCenterOfMassPosition().GetX(),
                body.GetCenterOfMassPosition().GetY(),
                body.GetCenterOfMassPosition().GetZ()
            );
            ImGui::LabelText(
                "World Space Bounds",
                "(%.2f, %.2f, %.2f) - (%.2f, %.2f, %.2f)",
                body.GetWorldSpaceBounds().mMin.GetX(),
                body.GetWorldSpaceBounds().mMin.GetY(),
                body.GetWorldSpaceBounds().mMin.GetZ(),
                body.GetWorldSpaceBounds().mMax.GetX(),
                body.GetWorldSpaceBounds().mMax.GetY(),
                body.GetWorldSpaceBounds().mMax.GetZ()
            );
        }
    }

private:
    static const char _register;

    bool DrawPhysicsLuaCallbackInspector(std::vector<PhysicsLuaCallback>& callbacks)
    {
        bool changed = false;
        for (size_t i = 0; i < callbacks.size(); ++i)
        {
            bool wantRemove = false;
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Button("x"))
            {
                wantRemove = true;
            }
            ImGui::SameLine();
            ImGui::Text("%zu", i);
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

        if (ImGui::Button("Add Callback"))
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
