#include "Editor/EditorState.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include <iterator>

namespace Editor
{
namespace
{
struct TerrainConfigState
{
    ObjPtr<BinaryAsset> heightAsset;
    ObjPtr<BinaryAsset> layerControlAsset;
    float2 size;
    float2 heightRange;
    uint32_t heightResolution;
    uint32_t vertexResolution;
    float3 baseColor;
    float roughness;
    float metallic;
    std::vector<uint16_t> heightSamples;
    uint32_t layerControlResolution;
    std::vector<TerrainLayerControlSample> layerIDSamples;
    std::vector<TerrainLayerControlSample> layerWeightSamples;

    static TerrainConfigState Capture(TerrainConfig& config)
    {
        TerrainConfigState state;
        state.heightAsset = config.GetHeightDataAsset();
        state.layerControlAsset = config.GetLayerControlDataAsset();
        state.size = config.GetSize();
        state.heightRange = config.GetHeightRange();
        state.heightResolution = config.GetHeightMapResolution();
        state.vertexResolution = config.GetVertexResolution();
        state.baseColor = config.GetBaseColor();
        state.roughness = config.GetRoughness();
        state.metallic = config.GetMetallic();
        state.heightSamples.assign(config.GetHeightSamples().begin(), config.GetHeightSamples().end());
        state.layerControlResolution = config.GetLayerControlResolution();
        state.layerIDSamples.assign(config.GetLayerIDSamples().begin(), config.GetLayerIDSamples().end());
        state.layerWeightSamples.assign(config.GetLayerWeightSamples().begin(), config.GetLayerWeightSamples().end());
        return state;
    }

    bool Matches(const TerrainConfigState& other) const
    {
        return heightAsset.Get() == other.heightAsset.Get() &&
               layerControlAsset.Get() == other.layerControlAsset.Get() && size == other.size &&
               heightRange == other.heightRange && heightResolution == other.heightResolution &&
               vertexResolution == other.vertexResolution && baseColor == other.baseColor &&
               roughness == other.roughness && metallic == other.metallic &&
               heightSamples == other.heightSamples &&
               layerControlResolution == other.layerControlResolution &&
               layerIDSamples == other.layerIDSamples && layerWeightSamples == other.layerWeightSamples;
    }
};

class TerrainConfigDataCommand final : public UndoCommand
{
public:
    TerrainConfigDataCommand(
        std::string name,
        TerrainConfig* config,
        TerrainConfigState before,
        TerrainConfigState after
    )
        : name(std::move(name)), config(config), before(std::move(before)), after(std::move(after)) {}

    void Undo() override { Apply(before); }
    void Redo() override { Apply(after); }
    const std::string& GetName() const override { return name; }

private:
    std::string name;
    ObjPtr<TerrainConfig> config;
    TerrainConfigState before;
    TerrainConfigState after;

    void Apply(const TerrainConfigState& state)
    {
        TerrainConfig* target = config.Get();
        if (target == nullptr)
            return;

        target->SetHeightDataAsset(state.heightAsset.Get());
        target->SetLayerControlDataAsset(state.layerControlAsset.Get());
        target->SetSize(state.size);
        target->SetHeightRange(state.heightRange, false);
        if (target->GetHeightMapResolution() != state.heightResolution)
            target->ResizeHeightMap(state.heightResolution);
        target->SetVertexResolution(state.vertexResolution);
        target->SetSurface(state.baseColor, state.roughness, state.metallic);

        if (state.heightSamples.size() ==
            static_cast<size_t>(state.heightResolution) * state.heightResolution)
        {
            std::vector<TerrainHeightValue> values;
            values.reserve(state.heightSamples.size());
            for (uint32_t i = 0; i < state.heightSamples.size(); ++i)
                values.push_back({i, state.heightSamples[i]});
            target->ApplyHeightValues(values);
            target->CommitHeightData();
        }

        if (target->GetLayerControlResolution() != state.layerControlResolution)
            target->ResizeLayerControlMaps(state.layerControlResolution);
        if (target->SetLayerControlSamples(state.layerIDSamples, state.layerWeightSamples))
            target->CommitLayerControlData();
    }
};

} // namespace

class TerrainConfigInspector final : public Inspector<TerrainConfig>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        (void)editor;
        auto capturePropertyChange = [this](const std::string& name, const std::function<void()>& change)
        {
            EditorState::GetUndoManager().CaptureAssetChange(name, {target.Get()}, change);
        };
        auto captureDataChange = [this](const std::string& name, const std::function<void()>& change)
        {
            TerrainConfigState before = TerrainConfigState::Capture(*target);
            change();
            TerrainConfigState after = TerrainConfigState::Capture(*target);
            if (before.Matches(after))
                return;
            EditorState::GetUndoManager().Execute(std::make_unique<TerrainConfigDataCommand>(
                name,
                target.Get(),
                std::move(before),
                std::move(after)
            ));
        };

        std::string name = target->GetName();
        if (EditorGUI::InputText("Name", name))
            capturePropertyChange("Rename Terrain Config", [this, name]()
                                  { target->SetName(name); });
        EditorGUI::Text("UUID", target->GetUUID().ToString());

        BinaryAsset* heightAsset = target->GetHeightDataAsset();
        if (EditorGUI::ObjectField("Height Data", heightAsset))
            captureDataChange("Set Terrain Height Data", [this, heightAsset]()
                              { target->SetHeightDataAsset(heightAsset); });
        ImGui::Text("Storage: R16_UNorm, %u x %u", target->GetHeightMapResolution(), target->GetHeightMapResolution());
        if (!target->HasValidHeightData())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "Height data is missing or incompatible.");

        EditorGUI::SeparatorTextLabeled("Geometry");
        ImGuiStorage* inspectorStorage = ImGui::GetStateStorage();
        const ImGuiID sizeDraftXId = ImGui::GetID("Terrain Size Draft X");
        const ImGuiID sizeDraftYId = ImGui::GetID("Terrain Size Draft Y");
        const ImGuiID sizeDraftDirtyId = ImGui::GetID("Terrain Size Draft Dirty");
        const bool sizeDraftDirty = inspectorStorage->GetBool(sizeDraftDirtyId, false);
        float2 size = sizeDraftDirty
                          ? float2(inspectorStorage->GetFloat(sizeDraftXId), inspectorStorage->GetFloat(sizeDraftYId))
                          : target->GetSize();
        if (ImGui::InputFloat2("Size", &size.x, "%.2f"))
        {
            inspectorStorage->SetFloat(sizeDraftXId, size.x);
            inspectorStorage->SetFloat(sizeDraftYId, size.y);
            inspectorStorage->SetBool(sizeDraftDirtyId, true);
        }

        const bool canRescale = size.x > 0.0f && size.y > 0.0f && size != target->GetSize();
        if (!canRescale)
            ImGui::BeginDisabled();
        if (ImGui::Button("Rescale"))
        {
            capturePropertyChange("Rescale Terrain", [this, size]()
                                  { target->SetSize(size); });
            inspectorStorage->SetBool(sizeDraftDirtyId, false);
        }
        if (!canRescale)
            ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Rebuilds the runtime terrain mesh.");

        float2 range = target->GetHeightRange();
        if (ImGui::InputFloat2("Height Range", &range.x, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
            captureDataChange("Set Terrain Height Range", [this, range]()
                              { target->SetHeightRange(range, true); });

        static constexpr uint32_t HeightResolutions[] = {128, 256, 512, 1024, 2048};
        static constexpr const char* HeightResolutionNames[] = {"128", "256", "512", "1024", "2048"};
        int heightIndex = 0;
        for (int i = 0; i < static_cast<int>(std::size(HeightResolutions)); ++i)
            if (HeightResolutions[i] == target->GetHeightMapResolution())
                heightIndex = i;
        if (ImGui::Combo("Height Map Resolution", &heightIndex, HeightResolutionNames, std::size(HeightResolutionNames)))
        {
            const uint32_t resolution = HeightResolutions[heightIndex];
            captureDataChange("Resize Terrain Height Map", [this, resolution]()
                              { target->ResizeHeightMap(resolution); });
        }

        static constexpr uint32_t VertexResolutions[] = {17, 33, 65, 129, 257, 513};
        static constexpr const char* VertexResolutionNames[] = {"17", "33", "65", "129", "257", "513"};
        int vertexIndex = 0;
        for (int i = 0; i < static_cast<int>(std::size(VertexResolutions)); ++i)
            if (VertexResolutions[i] == target->GetVertexResolution())
                vertexIndex = i;
        if (ImGui::Combo("Vertex Resolution", &vertexIndex, VertexResolutionNames, std::size(VertexResolutionNames)))
        {
            const uint32_t resolution = VertexResolutions[vertexIndex];
            capturePropertyChange("Set Terrain Vertex Resolution", [this, resolution]()
                                  { target->SetVertexResolution(resolution); });
        }

        EditorGUI::SeparatorTextLabeled("Surface");
        float3 baseColor = target->GetBaseColor();
        float roughness = target->GetRoughness();
        float metallic = target->GetMetallic();
        bool surfaceChanged = ImGui::ColorEdit3("Base Color", &baseColor.x);
        surfaceChanged |= ImGui::SliderFloat("Roughness", &roughness, 0.01f, 0.99f);
        surfaceChanged |= ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);
        if (surfaceChanged)
        {
            capturePropertyChange("Edit Terrain Surface", [this, baseColor, roughness, metallic]()
                                  { target->SetSurface(baseColor, roughness, metallic); });
        }

        ImGui::TextDisabled("Used when no valid terrain layer contributes to a texel.");

        EditorGUI::SeparatorTextLabeled("Layer Controls");
        BinaryAsset* layerControlAsset = target->GetLayerControlDataAsset();
        if (EditorGUI::ObjectField("Control Data", layerControlAsset))
            captureDataChange("Set Terrain Layer Control Data", [this, layerControlAsset]()
                              { target->SetLayerControlDataAsset(layerControlAsset); });
        ImGui::Text(
            "Storage: RGBA8 IDs + RGBA8 weights, %u x %u",
            target->GetLayerControlResolution(),
            target->GetLayerControlResolution()
        );
        ImGui::TextDisabled("IDs and weights use point-clamp sampling; layer ID 255 is invalid.");
        if (!target->HasValidLayerControlData())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "Layer control data is missing or incompatible.");

        int controlResolutionIndex = 0;
        for (int i = 0; i < static_cast<int>(std::size(HeightResolutions)); ++i)
            if (HeightResolutions[i] == target->GetLayerControlResolution())
                controlResolutionIndex = i;
        if (ImGui::Combo(
                "Control Resolution",
                &controlResolutionIndex,
                HeightResolutionNames,
                std::size(HeightResolutionNames)
            ))
        {
            const uint32_t resolution = HeightResolutions[controlResolutionIndex];
            captureDataChange("Resize Terrain Layer Controls", [this, resolution]()
                              { target->ResizeLayerControlMaps(resolution); });
        }
        if (ImGui::Button("Reset Layer Controls"))
            ImGui::OpenPopup("Reset Terrain Layer Controls");
        if (ImGui::BeginPopupModal("Reset Terrain Layer Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Assign layer 0 with full weight to every control texel?");
            if (ImGui::Button("Reset"))
            {
                captureDataChange("Reset Terrain Layer Controls", [this]()
                                  { target->InitializeLayerControlMaps(); });
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        EditorGUI::SeparatorTextLabeled("Terrain Layers");
        ImGui::Text("%zu / %u layer definitions", target->GetLayers().size(), TerrainConfig::MaxTerrainLayers);
        if (ImGui::Button("Add Layer"))
            capturePropertyChange("Add Terrain Layer", [this]()
                                  { target->AddLayer(); });

        uint32_t layerToRemove = TerrainConfig::InvalidTerrainLayerID;
        for (const TerrainLayer& storedLayer : target->GetLayers())
        {
            TerrainLayer layer = storedLayer;
            ImGui::PushID(static_cast<int>(layer.id));
            const std::string header = layer.name + " (ID " + std::to_string(layer.id) + ")###TerrainLayerHeader";
            if (ImGui::CollapsingHeader(header.c_str()))
            {
                auto applyLayer = [this, &capturePropertyChange](const char* commandName, const TerrainLayer& value)
                {
                    capturePropertyChange(commandName, [this, value]()
                                          { target->SetLayer(value); });
                };

                if (EditorGUI::InputText("Name", layer.name))
                    applyLayer("Rename Terrain Layer", layer);

                Texture* albedoRoughness = layer.albedoRoughnessTexture.Get();
                if (EditorGUI::ObjectField("Albedo + Roughness", albedoRoughness))
                {
                    layer.albedoRoughnessTexture = albedoRoughness;
                    applyLayer("Set Terrain Layer Albedo", layer);
                }
                Texture* normal = layer.normalTexture.Get();
                if (EditorGUI::ObjectField("Detail Normal", normal))
                {
                    layer.normalTexture = normal;
                    applyLayer("Set Terrain Layer Normal", layer);
                }
                Texture* height = layer.heightTexture.Get();
                if (EditorGUI::ObjectField("Parallax Height", height))
                {
                    layer.heightTexture = height;
                    applyLayer("Set Terrain Layer Height", layer);
                }
                Texture* layerMetallic = layer.metallicTexture.Get();
                if (EditorGUI::ObjectField("Metallic", layerMetallic))
                {
                    layer.metallicTexture = layerMetallic;
                    applyLayer("Set Terrain Layer Metallic", layer);
                }
                if (ImGui::InputFloat2("Tile Size (m)", &layer.tileSize.x, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
                    applyLayer("Set Terrain Layer Tile Size", layer);
                if (ImGui::InputFloat(
                        "Parallax Depth (m)",
                        &layer.parallaxDepth,
                        0.0f,
                        0.0f,
                        "%.3f",
                        ImGuiInputTextFlags_EnterReturnsTrue
                    ))
                    applyLayer("Set Terrain Layer Parallax Depth", layer);

                const bool ready = albedoRoughness != nullptr && normal != nullptr && height != nullptr &&
                                   layerMetallic != nullptr &&
                                   layer.tileSize.x > 0.0f && layer.tileSize.y > 0.0f;
                if (!ready)
                    ImGui::TextDisabled("All four textures and a positive tile size are required for rendering.");
                if (ImGui::Button("Remove Layer"))
                    layerToRemove = layer.id;
            }
            ImGui::PopID();
        }
        if (layerToRemove != TerrainConfig::InvalidTerrainLayerID)
            capturePropertyChange("Remove Terrain Layer", [this, layerToRemove]()
                                  { target->RemoveLayer(layerToRemove); });

        EditorGUI::SeparatorTextLabeled("Height Map");
        if (ImGui::Button("Reset To Flat"))
            ImGui::OpenPopup("Reset Terrain Height Map");
        if (ImGui::BeginPopupModal("Reset Terrain Height Map", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Replace every height sample with world height 0?");
            if (ImGui::Button("Reset"))
            {
                captureDataChange("Reset Terrain Height Map", [this]()
                                  { target->InitializeFlatHeightMap(); });
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

private:
    static const char _register;
};

const char TerrainConfigInspector::_register = InspectorRegistry::Register<TerrainConfigInspector, TerrainConfig>();
} // namespace Editor
