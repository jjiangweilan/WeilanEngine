#include "Editor/EditorState.hpp"
#include "../Inspector.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/Light.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"

namespace Editor
{
class LightInspector : public Inspector<Light>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Light* light = target;
        glm::vec3 lightColor = light->GetLightColor();
        float intensity = light->GetIntensity();
        float ambientScale = light->GetAmbientScale();
        int lightType = static_cast<int>(light->GetLightType());
        float shadowDistance = light->GetShadowDistance();
        const char* lightTypes[] = {"Directional", "Point"};

        EditorGUI::ComboLabeled("Light Type", &lightType, lightTypes, IM_ARRAYSIZE(lightTypes));
        if (light->GetLightType() != static_cast<LightType>(lightType))
        {
            light->SetLightType(static_cast<LightType>(lightType));
        }

        if (EditorGUI::DragFloat("Shadow Distance", &shadowDistance))
        {
            light->SetShadowDistance(shadowDistance);
        }

        if (EditorGUI::DragFloat("Ambient Scale", &ambientScale))
        {
            light->SetAmbientScale(ambientScale);
        }

        float wrappedDiffuse = light->GetWrappedDiffuse();
        if (EditorGUI::DragFloat("Wrapped Diffuse", &wrappedDiffuse))
        {
            light->SetWrappedDiffuse(wrappedDiffuse);
        }

        EditorGUI::DragFloat("Depth Bias", &target->depthBias);
        EditorGUI::DragFloat("Depth Slope Bias", &target->depthSlopeBias);

        if (ImGui::BeginTable("##lightcolor_table", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);
            if (ImGui::ColorPicker3("##lightcolor", &lightColor[0]))
            {
                light->SetLightColor(lightColor);
            }

            ImGui::EndTable();
        }
        if (EditorGUI::DragFloat("Intensity", &intensity))
        {
            light->SetIntensity(intensity);
        }

        if (light->GetLightType() == LightType::Point)
        {
            float v1 = light->GetPointLightDistance();
            if (EditorGUI::DragFloat("Point Light Distance", &v1))
            {
                light->SetPointLightDistance(v1);
            }
        }

        EditorGUI::SeparatorTextLabeled("Shadow Cache");
        if (!light->IsShadowCacheEnabled())
        {
            if (EditorGUI::ButtonSimple("Enable Shadow Cache"))
            {
                light->EnableShadowCache();
            }
        }
        else
        {
            if (EditorGUI::ButtonSimple("Disable Shadow Cache"))
            {
                light->DisableShadowCache();
            }
        }

        int targetFrames = light->GetShadowCacheTargetFrames();
        if (EditorGUI::DragInt("Target Frames", &targetFrames))
        {
            light->SetShadowUpdateFrames(targetFrames);
        }
        ImGui::Separator();

        EditorGUI::SeparatorTextLabeled("Shadow Cascades");
        bool cascadeEnabled = light->IsCascadeShadowEnabled();
        if (ImGui::Checkbox("Enable Cascaded Shadows", &cascadeEnabled))
        {
            light->SetShadowCascadeEnabled(cascadeEnabled);
        }

        if (cascadeEnabled)
        {
            auto cascades = light->GetShadowCascadeSplits();
            int cascadeCount = static_cast<int>(cascades.size());
            
            if (EditorGUI::DragInt("Cascade Count", &cascadeCount, 1.0f, 1, 4))
            {
                std::vector<ShadowCascade> newCascades;
                for (int i = 0; i < cascadeCount; ++i)
                {
                    if (i < static_cast<int>(cascades.size()))
                    {
                        newCascades.push_back(cascades[i]);
                    }
                    else
                    {
                        newCascades.push_back({(i + 1) * 50.0f});
                    }
                }
                light->SetCascadeShadowSplits(newCascades);
                cascades = newCascades;
            }

            for (int i = 0; i < static_cast<int>(cascades.size()); ++i)
            {
                float splitDistance = cascades[i].splitDistance;
                std::string label = "Cascade " + std::to_string(i + 1) + " Distance";
                if (EditorGUI::DragFloat(label.c_str(), &splitDistance, 1.0f, 0.1f, 1000.0f))
                {
                    auto updatedCascades = light->GetShadowCascadeSplits();
                    updatedCascades[i].splitDistance = splitDistance;
                    light->SetCascadeShadowSplits(updatedCascades);
                }
            }
        }
        ImGui::Separator();

        // Frustum frustum = target->GetLightFrusutmPlanes(target->GetGameObject()->GetPosition());

        // // 6 different float4 colors
        // float4 colors[6] = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}, {0, 1, 1, 1}, {1, 0, 1, 1}};

        // auto cam = target->GetScene()->GetMainCamera();
        // for (int i = 0; i < 6; ++i)
        // {
        //     Graphics::DrawLine(
        //         cam->GetGameObject()->GetPosition(),
        //         -float3(frustum.planes[i]) * frustum.planes[i].a + cam->GetGameObject()->GetPosition(),
        //         colors[i]
        //     );
        // }
    }

private:
    static const char _register;
};

const char LightInspector::_register = InspectorRegistry::Register<LightInspector, Light>();

} // namespace Editor
