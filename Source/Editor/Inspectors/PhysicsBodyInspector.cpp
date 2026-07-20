#include "Editor/EditorState.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/Runtime/System/SceneManager/PhysicsScene.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Jolt/Physics/Body/Body.h"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{
class PhysicsBodyInspector : public Inspector<PhysicsBody>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        /** Sensor **/
        bool isSensor = target->IsSensor();
        if (EditorGUI::Checkbox("Is Sensor", &isSensor))
        {
            target->SetSensor(isSensor);
        }

        const char* items[] = {"Static", "Dynamic", "Sprite", "Sensor"}; // defined in PhysicsLayer.hpp
        int currentItenIndex = static_cast<int>(target->GetLayer());
        if (EditorGUI::ComboLabeled("Layer", &currentItenIndex, items, IM_ARRAYSIZE(items)))
        {
            if (currentItenIndex == 0)
                target->SetLayer(PhysicsObjectLayers::Static);
            else if (currentItenIndex == 1)
                target->SetLayer(PhysicsObjectLayers::Dynamic);
            else if (currentItenIndex == 2)
                target->SetLayer(PhysicsObjectLayers::Sprite);
            else if (currentItenIndex == 3)
                target->SetLayer(PhysicsObjectLayers::Sensor);
        }

        /** Shape **/
        const char* shapes[] = {"Box", "Sphere", "Mesh", "Capsule", "Compound", "Terrain"};
        int currentShapeIndex = static_cast<int>(target->GetShape());
        glm::vec4 bodyScale = target->GetBodyScale();
        if (EditorGUI::ComboLabeled("Shape", &currentShapeIndex, shapes, IM_ARRAYSIZE(shapes)))
        {
            if (currentShapeIndex == 0)
                target->SetShape(PhysicsBodyShapes::Box);
            else if (currentShapeIndex == 1)
                target->SetShape(PhysicsBodyShapes::Sphere);
            else if (currentShapeIndex == 2)
                target->SetShape(PhysicsBodyShapes::Mesh);
            else if (currentShapeIndex == 3)
                target->SetShape(PhysicsBodyShapes::Capsule);
            else if (currentShapeIndex == 4)
                target->SetShape(PhysicsBodyShapes::Compound);
            else if (currentShapeIndex == 5)
                target->SetShape(PhysicsBodyShapes::Terrain);
        }

        /** Position **/
        auto bodyOffset = target->GetBodyOffset();
        if (EditorGUI::DragFloat3("Body Offset", &bodyOffset[0]))
        {
            target->SetBodyOffset(bodyOffset);
        }

        /** Scale **/
        if (currentShapeIndex == 0)
        {
            if (EditorGUI::DragFloat3("Box Scale", &bodyScale[0]))
                target->SetBodyScale(bodyScale);
        }
        else if (currentShapeIndex == 1)
        {
            if (EditorGUI::DragFloat("Sphere Scale", &bodyScale[0]))
            {
                target->SetBodyScale(bodyScale);
            }
        }
        else if (currentShapeIndex == 3)
        {
            if (EditorGUI::DragFloat("Capsule Height", &bodyScale[0]))
            {
                target->SetBodyScale(bodyScale);
            }

            if (EditorGUI::DragFloat("Capsule Radius", &bodyScale[1]))
            {
                target->SetBodyScale(bodyScale);
            }
        }

        // gravity factor
        float gf = target->GetGravityFactory();
        if (EditorGUI::DragFloat("Gravity Factor", &gf))
        {
            target->SetGravityFactor(gf);
        }

        // motion type
        JPH::EMotionType motionType = target->GetMotionType();
        int currentMotionType = static_cast<int>(motionType);
        const char* motionTypes[] = {"Static", "Kinematic", "Dynamic"};
        ImGui::BeginDisabled(target->GetShape() == PhysicsBodyShapes::Terrain);
        if (EditorGUI::ComboLabeled("Motion Type", &currentMotionType, motionTypes, IM_ARRAYSIZE(motionTypes)))
        {
            target->SetMotionType(static_cast<JPH::EMotionType>(currentMotionType));
        }
        ImGui::EndDisabled();

        JPH::EAllowedDOFs allowedDOFs = target->GetAllowedDOFs();
        auto drawAxisLock = [&](const char* id, JPH::EAllowedDOFs axis)
        {
            bool locked = (allowedDOFs & axis) == JPH::EAllowedDOFs::None;
            const bool wouldLockAllAxes = !locked && allowedDOFs == axis;
            ImGui::BeginDisabled(wouldLockAllAxes);
            const float checkboxWidth = ImGui::GetFrameHeight();
            const float columnWidth = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (columnWidth - checkboxWidth) * 0.5f);
            if (ImGui::Checkbox(id, &locked))
            {
                if (locked)
                    allowedDOFs &= ~axis;
                else
                    allowedDOFs |= axis;

                if (allowedDOFs != JPH::EAllowedDOFs::None)
                    target->SetAllowedDOFs(allowedDOFs);
            }
            ImGui::EndDisabled();
        };

        EditorGUI::SeparatorTextLabeled("Constraints");
        if (ImGui::BeginTable("PhysicsBodyConstraints", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Position");
            ImGui::TableSetColumnIndex(1);
            drawAxisLock("##LockPositionX", JPH::EAllowedDOFs::TranslationX);
            ImGui::TableSetColumnIndex(2);
            drawAxisLock("##LockPositionY", JPH::EAllowedDOFs::TranslationY);
            ImGui::TableSetColumnIndex(3);
            drawAxisLock("##LockPositionZ", JPH::EAllowedDOFs::TranslationZ);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Rotation");
            ImGui::TableSetColumnIndex(1);
            drawAxisLock("##LockRotationX", JPH::EAllowedDOFs::RotationX);
            ImGui::TableSetColumnIndex(2);
            drawAxisLock("##LockRotationY", JPH::EAllowedDOFs::RotationY);
            ImGui::TableSetColumnIndex(3);
            drawAxisLock("##LockRotationZ", JPH::EAllowedDOFs::RotationZ);

            ImGui::EndTable();
        }

        if (motionType == JPH::EMotionType::Kinematic)
        {
            bool shouldKinematicGenerateContactPointsWithNonDynamic =
                target->ShouldKinematicGenerateContactPointsWithNonDynamic();
            if (EditorGUI::Checkbox("Collide With NonDynamic", &shouldKinematicGenerateContactPointsWithNonDynamic))
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

        EditorGUI::SeparatorTextLabeled("Status");
        auto body = target->GetBody();
        if (body)
        {
            EditorGUI::TextFormatted("ID", "%i", body->GetID().GetIndexAndSequenceNumber());
            EditorGUI::Text("Body Type", body->GetBodyType() == JPH::EBodyType::RigidBody ? "Rigid" : "Soft");
            EditorGUI::Text("Is Active", body->IsActive() ? "True" : "False");
            EditorGUI::Text("Motion Type", motionTypes[currentMotionType]);
            EditorGUI::Text("Is Sensor", body->IsSensor() ? "True" : "False");
            EditorGUI::Text("Can Be Kinematic or Dynamic", body->CanBeKinematicOrDynamic() ? "True" : "False");
            EditorGUI::Text("Use Manifold Reduction", body->GetUseManifoldReduction() ? "True" : "False");
            EditorGUI::Text("Broad Phase Layer", MapBroadPhaseLayerToString(body->GetBroadPhaseLayer()));
            EditorGUI::TextFormatted("Object Layer", "%d", body->GetObjectLayer());
            EditorGUI::TextFormatted("Friction", "%.2f", body->GetFriction());
            EditorGUI::TextFormatted("Restitution", "%.2f", body->GetRestitution());
            EditorGUI::TextFormatted("User Data", "%llu", body->GetUserData());

            if (!body->IsStatic() && body->GetMotionProperties())
            {
                EditorGUI::Text("Allow Sleeping", body->GetAllowSleeping() ? "True" : "False");
                EditorGUI::TextFormatted("Linear Velocity", "(%.2f, %.2f, %.2f)",
                    body->GetLinearVelocity().GetX(),
                    body->GetLinearVelocity().GetY(),
                    body->GetLinearVelocity().GetZ()
                );
                EditorGUI::TextFormatted("Angular Velocity", "(%.2f, %.2f, %.2f)",
                    body->GetAngularVelocity().GetX(),
                    body->GetAngularVelocity().GetY(),
                    body->GetAngularVelocity().GetZ()
                );
                if (body->IsDynamic())
                {
                    EditorGUI::TextFormatted("Accumulated Force", "(%.2f, %.2f, %.2f)",
                        body->GetAccumulatedForce().GetX(),
                        body->GetAccumulatedForce().GetY(),
                        body->GetAccumulatedForce().GetZ()
                    );

                    EditorGUI::TextFormatted("Accumulated Torque", "(%.2f, %.2f, %.2f)",
                        body->GetAccumulatedTorque().GetX(),
                        body->GetAccumulatedTorque().GetY(),
                        body->GetAccumulatedTorque().GetZ()
                    );
                }
                EditorGUI::Text("In Broad Phase", body->IsInBroadPhase() ? "True" : "False");
                EditorGUI::Text("Collision Cache Invalid", body->IsCollisionCacheInvalid() ? "True" : "False");
                EditorGUI::Text("Shape", body->GetShape() ? "Valid" : "Null");
                EditorGUI::TextFormatted("Position", "(%.2f, %.2f, %.2f)",
                    body->GetPosition().GetX(),
                    body->GetPosition().GetY(),
                    body->GetPosition().GetZ()
                );
                EditorGUI::TextFormatted("Rotation", "(%.2f, %.2f, %.2f, %.2f)",
                    body->GetRotation().GetX(),
                    body->GetRotation().GetY(),
                    body->GetRotation().GetZ(),
                    body->GetRotation().GetW()
                );
                EditorGUI::TextFormatted("Center of Mass Position", "(%.2f, %.2f, %.2f)",
                    body->GetCenterOfMassPosition().GetX(),
                    body->GetCenterOfMassPosition().GetY(),
                    body->GetCenterOfMassPosition().GetZ()
                );
                EditorGUI::TextFormatted("World Space Bounds", "(%.2f, %.2f, %.2f) - (%.2f, %.2f, %.2f)",
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

    bool DrawPhysicsLuaCallbackInspector(std::vector<PhysicsLuaCallback>& callbacks)
    {
        bool changed = false;
        for (size_t i = 0; i < callbacks.size(); ++i)
        {
            bool wantRemove = false;
            ImGui::PushID(static_cast<int>(i));
            EditorGUI::TextFormatted("Index", "%zu", i + 1);
            ImGui::SameLine();
            if (EditorGUI::ButtonSimple("x"))
            {
                wantRemove = true;
            }
            ImGui::SameLine();
            GameScript* gameScript = callbacks[i].gameScript;
            if (EditorGUI::ObjectField("", gameScript))
            {
                changed = true;
                callbacks[i].gameScript = gameScript;
            }
            ImGui::SameLine();
            if (EditorGUI::InputText("##Callback", callbacks[i].callback, "Function Name"))
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

        if (EditorGUI::ButtonSimple("Add Callback"))
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
