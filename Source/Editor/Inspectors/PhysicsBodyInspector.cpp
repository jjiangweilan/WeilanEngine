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

        ImGui::SeparatorText("Status");
        auto body = target->GetBody();
        if (body && ImGui::BeginTable("StatusTable", 2))
        {
            ImGui::TableNextColumn();
            ImGui::Text("ID");
            ImGui::TableNextColumn();
            ImGui::Text("%i", body->GetID().GetIndexAndSequenceNumber());
            ImGui::TableNextColumn();
            ImGui::Text("Body Type");
            ImGui::TableNextColumn();
            ImGui::Text("%s", body->GetBodyType() == JPH::EBodyType::RigidBody ? "Rigid" : "Soft");
            ImGui::TableNextColumn();
            ImGui::Text("Is Active");
            ImGui::TableNextColumn();
            ImGui::Text("%s", body->IsActive() ? "True" : "False");
            ImGui::TableNextColumn();
            ImGui::Text("Motion Type");
            ImGui::TableNextColumn();
            ImGui::Text("%s", motionTypes[currentMotionType]);
            ImGui::TableNextColumn();
            ImGui::Text("Is Sensor");
            ImGui::TableNextColumn();
            ImGui::Text("%s", body->IsSensor() ? "True" : "False");
            ImGui::TableNextColumn();
            ImGui::Text("Can Be Kinematic or Dynamic");
            ImGui::TableNextColumn();
            ImGui::Text("%s", body->CanBeKinematicOrDynamic() ? "True" : "False");
            ImGui::TableNextColumn();
            ImGui::Text("Use Manifold Reduction");
            ImGui::TableNextColumn();
            ImGui::Text("%s", body->GetUseManifoldReduction() ? "True" : "False");
            ImGui::TableNextColumn();
            ImGui::Text("Broad Phase Layer");
            ImGui::TableNextColumn();
            ImGui::Text("%s", BPLayerInterfaceImpl::GetBroadPhaseLayerNameImpl(body->GetBroadPhaseLayer()));
            ImGui::TableNextColumn();
            ImGui::Text("Object Layer");
            ImGui::TableNextColumn();
            ImGui::Text("%d", body->GetObjectLayer());
            ImGui::TableNextColumn();
            ImGui::Text("Friction");
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", body->GetFriction());
            ImGui::TableNextColumn();
            ImGui::Text("Restitution");
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", body->GetRestitution());
            ImGui::TableNextColumn();
            ImGui::Text("User Data");
            ImGui::TableNextColumn();
            ImGui::Text("%llu", body->GetUserData());

            if (!body->IsStatic() && body->GetMotionProperties())
            {
                ImGui::TableNextColumn();
                ImGui::Text("Allow Sleeping");
                ImGui::TableNextColumn();
                ImGui::Text("%s", body->GetAllowSleeping() ? "True" : "False");
                ImGui::TableNextColumn();
                ImGui::Text("Linear Velocity");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "(%.2f, %.2f, %.2f)",
                    body->GetLinearVelocity().GetX(),
                    body->GetLinearVelocity().GetY(),
                    body->GetLinearVelocity().GetZ()
                );
                ImGui::TableNextColumn();
                ImGui::Text("Angular Velocity");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "(%.2f, %.2f, %.2f)",
                    body->GetAngularVelocity().GetX(),
                    body->GetAngularVelocity().GetY(),
                    body->GetAngularVelocity().GetZ()
                );
                if (body->IsDynamic())
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("Accumulated Force");
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "(%.2f, %.2f, %.2f)",
                        body->GetAccumulatedForce().GetX(),
                        body->GetAccumulatedForce().GetY(),
                        body->GetAccumulatedForce().GetZ()
                    );

                    ImGui::TableNextColumn();
                    ImGui::Text("Accumulated Torque");
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "(%.2f, %.2f, %.2f)",
                        body->GetAccumulatedTorque().GetX(),
                        body->GetAccumulatedTorque().GetY(),
                        body->GetAccumulatedTorque().GetZ()
                    );
                }
                ImGui::TableNextColumn();
                ImGui::Text("In Broad Phase");
                ImGui::TableNextColumn();
                ImGui::Text("%s", body->IsInBroadPhase() ? "True" : "False");
                ImGui::TableNextColumn();
                ImGui::Text("Collision Cache Invalid");
                ImGui::TableNextColumn();
                ImGui::Text("%s", body->IsCollisionCacheInvalid() ? "True" : "False");
                ImGui::TableNextColumn();
                ImGui::Text("Shape");
                ImGui::TableNextColumn();
                ImGui::Text("%s", body->GetShape() ? "Valid" : "Null");
                ImGui::TableNextColumn();
                ImGui::Text("Position");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "(%.2f, %.2f, %.2f)",
                    body->GetPosition().GetX(),
                    body->GetPosition().GetY(),
                    body->GetPosition().GetZ()
                );
                ImGui::TableNextColumn();
                ImGui::Text("Rotation");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "(%.2f, %.2f, %.2f, %.2f)",
                    body->GetRotation().GetX(),
                    body->GetRotation().GetY(),
                    body->GetRotation().GetZ(),
                    body->GetRotation().GetW()
                );
                ImGui::TableNextColumn();
                ImGui::Text("Center of Mass Position");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "(%.2f, %.2f, %.2f)",
                    body->GetCenterOfMassPosition().GetX(),
                    body->GetCenterOfMassPosition().GetY(),
                    body->GetCenterOfMassPosition().GetZ()
                );
                ImGui::TableNextColumn();
                ImGui::Text("World Space Bounds");
                ImGui::TableNextColumn();
                ImGui::Text(
                    "(%.2f, %.2f, %.2f) - (%.2f, %.2f, %.2f)",
                    body->GetWorldSpaceBounds().mMin.GetX(),
                    body->GetWorldSpaceBounds().mMin.GetY(),
                    body->GetWorldSpaceBounds().mMin.GetZ(),
                    body->GetWorldSpaceBounds().mMax.GetX(),
                    body->GetWorldSpaceBounds().mMax.GetY(),
                    body->GetWorldSpaceBounds().mMax.GetZ()
                );
            }

            ImGui::EndTable();
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
            ImGui::Text("%zu", i + 1);
            ImGui::SameLine();
            if (ImGui::Button("x"))
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
