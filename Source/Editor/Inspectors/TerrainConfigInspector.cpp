#include "Editor/EditorState.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include <iterator>

namespace Editor
{
namespace
{
struct TerrainConfigState
{
    ObjPtr<BinaryAsset> heightAsset;
    float2 size;
    float2 heightRange;
    uint32_t heightResolution;
    uint32_t vertexResolution;
    float3 baseColor;
    float roughness;
    float metallic;
    std::vector<uint16_t> heightSamples;

    static TerrainConfigState Capture(TerrainConfig& config)
    {
        TerrainConfigState state;
        state.heightAsset = config.GetHeightDataAsset();
        state.size = config.GetSize();
        state.heightRange = config.GetHeightRange();
        state.heightResolution = config.GetHeightMapResolution();
        state.vertexResolution = config.GetVertexResolution();
        state.baseColor = config.GetBaseColor();
        state.roughness = config.GetRoughness();
        state.metallic = config.GetMetallic();
        state.heightSamples.assign(config.GetHeightSamples().begin(), config.GetHeightSamples().end());
        return state;
    }

    bool Matches(const TerrainConfigState& other) const
    {
        return heightAsset.Get() == other.heightAsset.Get() && size == other.size &&
               heightRange == other.heightRange && heightResolution == other.heightResolution &&
               vertexResolution == other.vertexResolution && baseColor == other.baseColor &&
               roughness == other.roughness && metallic == other.metallic &&
               heightSamples == other.heightSamples;
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
