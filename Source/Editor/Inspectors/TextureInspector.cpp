#include "Editor/EditorState.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Editor/Inspectors/InspectorHelper_Image.hpp"

namespace Editor
{
class TextureInspector : public Inspector<Texture>
{
public:
    void OnEnable(Object& obj) override
    {
        Inspector<Texture>::OnEnable(obj);
        imageInspector.Initialize(target->GetGfxImage());
    }

    void DrawInspector(GameEditor& editor) override
    {
        // object information
        auto& name = target->GetName();
        char cname[1024];
        strcpy(cname, name.data());
        if (EditorGUI::InputTextLabeled("Name", cname, 1024))
        {
            target->SetName(cname);
        }
        EditorGUI::Text("UUID", target->GetUUID().ToString().c_str());

        if (reimport)
        {
            AssetDatabase::Singleton()->LoadAssetByID(target->GetUUID(), true);
        }

        ImGui::NewLine();
        EditorGUI::SeparatorTextLabeled("Preview");
        imageInspector.ShowImage(imageScale);
        ImGui::Separator();
        EditorGUI::DragFloat("Image Scale", &imageScale, 0.01, 0.01, 1.0);
        float mb = target->GetDescription().img.GetByteSize() / 1024.0f / 1024.0f;
        int targetDepth = target->GetDescription().img.depth;
        if (targetDepth > 1)
            EditorGUI::TextFormatted("Size", "%d x %d x %d", target->GetDescription().img.width, target->GetDescription().img.height, targetDepth);
        else
            EditorGUI::TextFormatted("Size", "%d x %d", target->GetDescription().img.width, target->GetDescription().img.height);
        EditorGUI::TextFormatted("Memory Size", "%.2f Mb", mb);
        EditorGUI::Text("Format", Gfx::MapGfxFormatToString(target->GetDescription().img.format));

        // show and update meta
        ImGui::NewLine();
        auto meta = AssetDatabase::Singleton()->GetAssetMeta(*target);
        auto options = meta.value("importOption", nlohmann::json::object());
        bool linearFormat = options.value("linearFormat", false);
        bool generateMipmap = options.value("generateMipmap", false);
        bool convertToIrradianceCubemap = options.value("convertToIrradianceCubemap", false);
        bool converToCubemap = options.value("convertToCubemap", false);
        bool convertToReflectanceCubemap = options.value("convertToReflectanceCubemap", false);

        EditorGUI::SeparatorTextLabeled("Import Options");
        bool metaChanged = false;
        metaChanged |= EditorGUI::Checkbox("Linear Format", &linearFormat);
        metaChanged |= EditorGUI::Checkbox("Generate Mipmap", &generateMipmap);
        metaChanged |= EditorGUI::Checkbox("Convert To Cubemap", &converToCubemap);
        metaChanged |= EditorGUI::Checkbox("Convert To Irradiance Cubemap", &convertToIrradianceCubemap);
        metaChanged |= EditorGUI::Checkbox("Convert To Reflectance Cubemap", &convertToReflectanceCubemap);
        if (metaChanged)
        {
            meta["importOption"]["generateMipmap"] = generateMipmap;
            meta["importOption"]["convertToCubemap"] = converToCubemap;
            meta["importOption"]["convertToIrradianceCubemap"] = convertToIrradianceCubemap;
            meta["importOption"]["convertToReflectanceCubemap"] = convertToReflectanceCubemap;
            meta["importOption"]["linearFormat"] = linearFormat;
        }
        if (metaChanged)
            AssetDatabase::Singleton()->SetAssetMeta(*target, meta);

        // import button
        if (EditorGUI::ButtonSimple("Reimport"))
        {
            reimport = true;
            imageInspector.SetReimport(true);
        }
    }

private:
    static const char _register;
    ImageInspector imageInspector;
    float imageScale = 1.0f;
    bool reimport = false;
};

const char TextureInspector::_register = InspectorRegistry::Register<TextureInspector, Texture>();

} // namespace Editor
