#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Engine/Modules/Ocean/OceanComponent.hpp"

namespace Editor
{
class OceanInspector : public Inspector<OceanComponent>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        OceanComponent* ocean = target;
        if (ocean == nullptr)
            return;

        auto& waves = ocean->GetWaves();
        
        EditorGUI::SeparatorTextLabeled("Ocean Waves");
        
        bool waveChanged = false;
        
        for (size_t i = 0; i < waves.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            
            if (ImGui::CollapsingHeader(("Wave " + std::to_string(i + 1)).c_str()))
            {
                if (EditorGUI::DragFloat2("Direction", &waves[i].direction[0], 0.01f))
                {
                    waves[i].direction = glm::normalize(waves[i].direction);
                    waveChanged = true;
                }
                
                if (EditorGUI::DragFloat("Amplitude", &waves[i].amplitude, 0.01f, 0.0f, 10.0f))
                {
                    waveChanged = true;
                }
                
                if (EditorGUI::DragFloat("Wavelength", &waves[i].wavelength, 0.1f, 0.1f, 100.0f))
                {
                    waveChanged = true;
                }
                
                if (EditorGUI::DragFloat("Speed", &waves[i].speed, 0.01f, 0.0f, 10.0f))
                {
                    waveChanged = true;
                }
                
                if (EditorGUI::DragFloat("Steepness", &waves[i].steepness, 0.01f, 0.0f, 1.0f))
                {
                    waveChanged = true;
                }
            }
            
            ImGui::PopID();
        }
        
        ImGui::Spacing();
        
        if (EditorGUI::ButtonSimple("Add Wave"))
        {
            waves.push_back({{1.0f, 0.0f}, 0.2f, 5.0f, 1.0f, 0.5f});
            waveChanged = true;
        }
        
        ImGui::SameLine();
        
        if (waves.size() > 0 && EditorGUI::ButtonSimple("Remove Last Wave"))
        {
            waves.pop_back();
            waveChanged = true;
        }
        
        if (waveChanged)
        {
            ocean->UpdateWaveBuffer();
        }
    }

private:
    static const char _register;
};

const char OceanInspector::_register = InspectorRegistry::Register<OceanInspector, OceanComponent>();

} // namespace Editor
