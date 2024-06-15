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
        const char* items[] = {"Scene", "Moving"}; // defined in PhysicsLayer.hpp
        int currentItenIndex = static_cast<int>(target->GetLayer());
        if (ImGui::Combo("combo", &currentItenIndex, items, IM_ARRAYSIZE(items)))
        {
            if (currentItenIndex == 0)
                target->SetLayer(PhysicsLayer::Scene);
            else if (currentItenIndex == 1)
                target->SetLayer(PhysicsLayer::Moving);
        }

        const char* shapes[] = {"Box", "Sphere", "Mesh"};
        int currentShapeIndex = static_cast<int>(target->GetShape());
        if (ImGui::Combo("Shape", &currentShapeIndex, shapes, IM_ARRAYSIZE(shapes)))
        {
            if (currentShapeIndex == 0)
            {
                target->SetShape(PhysicsBodyShapes::Box);
            }
            else if (currentShapeIndex == 1)
            {
                target->SetShape(PhysicsBodyShapes::Sphere);
            }
            else if (currentShapeIndex == 2)
            {
                target->SetShape(PhysicsBodyShapes::Mesh);
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

        target->debugDrawRequest = true;
    }

private:
    static const char _register;
};

const char PhysicsBodyInspector::_register = InspectorRegistry::Register<PhysicsBodyInspector, PhysicsBody>();

} // namespace Editor
