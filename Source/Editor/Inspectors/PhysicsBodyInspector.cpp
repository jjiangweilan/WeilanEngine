#include "../EditorState.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Inspector.hpp"

namespace Editor
{
class PhysicsBodyInspector : public Inspector<PhysicsBody>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        const char* items[] = {"Non Moving", "Moving"}; // defined in PhysicsLayer.hpp
        int currentItenIndex = static_cast<int>(target->GetLayer());
        if (ImGui::Combo("combo", &currentItenIndex, items, IM_ARRAYSIZE(items)))
        {
            if (currentItenIndex == 0)
                target->SetLayer(PhysicsLayer::NON_MOVING);
            else if (currentItenIndex == 1)
                target->SetLayer(PhysicsLayer::MOVING);
        }

        const char* shapes[] = {"Box", "Sphere"};
        int currentShapeIndex = 0;
        ImGui::Combo("Shape", &currentShapeIndex, shapes, IM_ARRAYSIZE(shapes));

        if (currentShapeIndex == 0)
        {
            auto bodyScale = target->GetBodyScale();
            if (ImGui::InputFloat3("half extent", &bodyScale[0]))
            {
                target->SetAsBox(bodyScale);
            }
        }
        else if (currentShapeIndex == 1)
        {
            auto bodyScale = target->GetBodyScale();
            if (ImGui::InputFloat("radius", &bodyScale[0]))
            {
                target->SetAsBox(bodyScale);
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
    }

private:
    static const char _register;
};

const char PhysicsBodyInspector::_register = InspectorRegistry::Register<PhysicsBodyInspector, PhysicsBody>();

} // namespace Editor
