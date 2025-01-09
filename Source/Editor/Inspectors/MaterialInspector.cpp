#include "../EditorState.hpp"
#include "EditorGUI.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Inspector.hpp"
#include "Rendering/EnumStringMapping.hpp"
#include "Rendering/Material.hpp"
namespace Editor
{
class MaterialInspector : public Inspector<Material>
{
public:
    void OnEnable(Object& obj) override
    {
        Inspector<Material>::OnEnable(obj);

        featureToEnable[0] = '\0';

        for (auto& obj : Object::GetAllEngineObjects())
        {
            if (obj.second->GetObjectTypeID() == Shader2::StaticGetObjectTypeID())
            {
                shaders.push_back(static_cast<Shader2*>(obj.second));
            }
        }

        std::sort(
            shaders.begin(),
            shaders.end(),
            [](Shader2* f, Shader2* s) { return f->GetShaderProgram()->GetName() < s->GetShaderProgram()->GetName(); }
        );
    }

    // void ShowFeatures(const std::vector<std::vector<std::string>>& features)
    //{
    //     for (auto& fs : features)
    //     {
    //         for (auto& f : fs)
    //         {
    //             if (f != ShaderBase::DefaultGlobalFeatureWord)
    //             {
    //                 bool enabled = target->IsFeatureEnabled(f);
    //                 ImGui::PushStyleColor(
    //                     ImGuiCol_Button,
    //                     enabled ? ImVec4(0.2, 0.7, 0.2, 1) : ImVec4(0.7, 0.2, 0.2, 1)
    //                 );
    //                 if (ImGui::Button(f.c_str()))
    //                 {
    //                     if (enabled)
    //                         target->DisableFeature(f);
    //                     else
    //                         target->EnableFeature(f);
    //                 }
    //                 ImGui::PopStyleColor();
    //             }
    //         }
    //     }
    // }

    void DrawInspector(GameEditor& editor) override
    {
        std::string name = target->GetName();
        if (GUI::InputText("Name", name))
            target->SetName(name);

        GUI::AutoObjectInspector(target);
    }

private:
    static const char _register;
    char featureToEnable[256];
    std::vector<Shader2*> shaders;

    glm::vec2 ResizeKeepRatio(float width, float height, float contentWidth, float contentHeight)
    {
        float imageWidth = width;
        float imageHeight = height;

        // shrink width
        if (imageWidth > contentWidth)
        {
            float ratio = contentWidth / (float)imageWidth;
            imageWidth = contentWidth;
            imageHeight *= ratio;
        }

        if (imageHeight > contentHeight)
        {
            float ratio = contentHeight / (float)imageHeight;
            imageHeight = contentHeight;
            imageWidth *= ratio;
        }

        return {imageWidth, imageHeight};
    }
};

const char MaterialInspector::_register = InspectorRegistry::Register<MaterialInspector, Material>();

} // namespace Editor
