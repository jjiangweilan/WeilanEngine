#include "../Inspector.hpp"
#include "Engine/Runtime/Object/Component/Boids.hpp"

namespace Editor
{
class BoidsInspector : public Inspector<Boids>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Boids>::DrawInspector(editor);

        auto& settings = target->GetSettings();

        ImGui::SeparatorText("Radii");
        EditorGUI::DragFloat("Separation Radius", &settings.separationRadius, 0.01f, 0.0f);
        EditorGUI::DragFloat("Alignment Radius", &settings.alignmentRadius, 0.01f, 0.0f);
        EditorGUI::DragFloat("Cohesion Radius", &settings.cohesionRadius, 0.01f, 0.0f);

        ImGui::SeparatorText("Weights");
        EditorGUI::DragFloat("Separation Weight", &settings.separationWeight, 0.01f, 0.0f);
        EditorGUI::DragFloat("Alignment Weight", &settings.alignmentWeight, 0.01f, 0.0f);
        EditorGUI::DragFloat("Cohesion Weight", &settings.cohesionWeight, 0.01f, 0.0f);

        ImGui::SeparatorText("Limits");
        EditorGUI::DragFloat("Max Speed", &settings.maxSpeed, 0.01f, 0.0f);
        EditorGUI::DragFloat("Max Force", &settings.maxForce, 0.01f, 0.0f);
    }

private:
    static const char _register;
};

const char BoidsInspector::_register = InspectorRegistry::Register<BoidsInspector, Boids>();
} // namespace Editor
