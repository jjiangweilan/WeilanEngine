#include "../EditorState.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "Inspector.hpp"
#include "InspectorHelper_Image.hpp"

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
        if (ImGui::InputText("Name", cname, 1024))
        {
            target->SetName(cname);
        }
        ImGui::Text("%s", target->GetUUID().ToString().c_str());

        if (reimport)
        {
            AssetDatabase::Singleton()->LoadAssetByID(target->GetUUID(), true);
        }

        ImGui::NewLine();
        ImGui::Text("Preview");
        ImGui::Separator();
        imageInspector.ShowImage(imageScale);
        ImGui::Separator();
        ImGui::DragFloat("Image Scale", &imageScale, 0.01, 0.01, 1.0);
        float mb = target->GetDescription().img.GetByteSize() / 1024.0f / 1024.0f;
        ImGui::Text("size: %d x %d", target->GetDescription().img.width, target->GetDescription().img.height);
        ImGui::Text("memory size (without mip): %f Mb", mb);
        ImGui::Text("format %s", Gfx::MapGfxFormatToString(target->GetDescription().img.format));

        // show and update meta
        ImGui::NewLine();
        auto meta = AssetDatabase::Singleton()->GetAssetMeta(*target);
        auto options = meta.value("importOption", nlohmann::json::object());
        bool linearFormat = options.value("linearFormat", false);
        bool generateMipmap = options.value("generateMipmap", false);
        bool convertToIrradianceCubemap = options.value("convertToIrradianceCubemap", false);
        bool converToCubemap = options.value("convertToCubemap", false);
        bool convertToReflectanceCubemap = options.value("convertToReflectanceCubemap", false);

        ImGui::Text("Import Options");
        ImGui::Separator();
        bool metaChanged = false;
        metaChanged |= ImGui::Checkbox("linearFormat", &linearFormat);
        metaChanged |= ImGui::Checkbox("generateMipmap", &generateMipmap);
        metaChanged |= ImGui::Checkbox("convertToCubemap", &converToCubemap);
        metaChanged |= ImGui::Checkbox("convertToIrradianceCubemap", &convertToIrradianceCubemap);
        metaChanged |= ImGui::Checkbox("convertToReflectanceCubemap", &convertToReflectanceCubemap);
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
        if (ImGui::Button("Reimport"))
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
