#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Runtime/Object/Component/ParticleSystem.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace Editor
{
namespace
{
template <class Enum>
bool DrawEnum(const char* label, Enum& value, const char* items)
{
    int selection = static_cast<int>(value);
    if (!ImGui::Combo(label, &selection, items))
        return false;
    value = static_cast<Enum>(selection);
    return true;
}

bool DrawCurve(const char* label, Particles::Curve& curve)
{
    bool changed = false;
    ImGui::PushID(label);
    if (ImGui::TreeNode("Curve"))
    {
        int removeIndex = -1;
        for (int i = 0; i < static_cast<int>(curve.keys.size()); ++i)
        {
            auto& key = curve.keys[i];
            ImGui::PushID(i);
            ImGui::Text("Key %d", i);
            ImGui::SameLine();
            if (curve.keys.size() > 1 && ImGui::SmallButton("Remove"))
                removeIndex = i;
            changed |= ImGui::DragFloat("Time", &key.time, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Value", &key.value, 0.01f);
            changed |= DrawEnum("Interpolation", key.interpolation, "Constant\0Linear\0Cubic\0");
            if (key.interpolation == Particles::CurveInterpolation::Cubic)
            {
                changed |= ImGui::DragFloat("In Tangent", &key.inTangent, 0.01f);
                changed |= ImGui::DragFloat("Out Tangent", &key.outTangent, 0.01f);
            }
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIndex >= 0)
        {
            curve.keys.erase(curve.keys.begin() + removeIndex);
            changed = true;
        }
        if (ImGui::SmallButton("Add Key"))
        {
            curve.keys.push_back({1.0f, curve.keys.empty() ? 1.0f : curve.keys.back().value});
            changed = true;
        }
        if (changed)
        {
            for (auto& key : curve.keys)
                key.time = std::clamp(key.time, 0.0f, 1.0f);
            curve.SortKeys();
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
    return changed;
}

bool DrawScalarParameter(const char* label, Particles::ScalarParameter& parameter)
{
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    changed |= DrawEnum("Mode", parameter.mode, "Constant\0Random Between Constants\0Curve\0Random Between Curves\0");
    switch (parameter.mode)
    {
        case Particles::ScalarMode::Constant:
            changed |= ImGui::DragFloat("Value", &parameter.constant, 0.01f);
            break;
        case Particles::ScalarMode::RandomBetweenConstants:
            changed |= ImGui::DragFloat("Minimum", &parameter.constantMin, 0.01f);
            changed |= ImGui::DragFloat("Maximum", &parameter.constantMax, 0.01f);
            break;
        case Particles::ScalarMode::Curve:
            changed |= DrawCurve("Value", parameter.curveMax);
            break;
        case Particles::ScalarMode::RandomBetweenCurves:
            changed |= DrawCurve("Minimum", parameter.curveMin);
            changed |= DrawCurve("Maximum", parameter.curveMax);
            break;
    }
    ImGui::Separator();
    ImGui::PopID();
    return changed;
}

bool DrawVectorParameter(const char* label, Particles::VectorParameter& parameter)
{
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    changed |= ImGui::Checkbox("Separate Axes", &parameter.separateAxes);
    if (parameter.separateAxes)
    {
        changed |= DrawScalarParameter("X", parameter.x);
        changed |= DrawScalarParameter("Y", parameter.y);
        changed |= DrawScalarParameter("Z", parameter.z);
    }
    else
    {
        changed |= DrawScalarParameter("Value", parameter.x);
    }
    ImGui::PopID();
    return changed;
}

bool DrawGradient(const char* label, Particles::Gradient& gradient)
{
    bool changed = false;
    ImGui::PushID(label);
    if (ImGui::TreeNode("Gradient"))
    {
        ImGui::TextUnformatted("Color Keys");
        int removeColor = -1;
        for (int i = 0; i < static_cast<int>(gradient.colorKeys.size()); ++i)
        {
            auto& key = gradient.colorKeys[i];
            ImGui::PushID(i);
            changed |= ImGui::DragFloat("Time", &key.time, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::ColorEdit3("Color", &key.color.x);
            ImGui::SameLine();
            if (gradient.colorKeys.size() > 1 && ImGui::SmallButton("Remove"))
                removeColor = i;
            ImGui::PopID();
        }
        if (removeColor >= 0)
        {
            gradient.colorKeys.erase(gradient.colorKeys.begin() + removeColor);
            changed = true;
        }
        if (ImGui::SmallButton("Add Color Key"))
        {
            gradient.colorKeys.push_back({1.0f, float3(1.0f)});
            changed = true;
        }

        ImGui::TextUnformatted("Alpha Keys");
        int removeAlpha = -1;
        for (int i = 0; i < static_cast<int>(gradient.alphaKeys.size()); ++i)
        {
            auto& key = gradient.alphaKeys[i];
            ImGui::PushID(i + 1000);
            changed |= ImGui::DragFloat("Time", &key.time, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Alpha", &key.alpha, 0.01f, 0.0f, 1.0f);
            ImGui::SameLine();
            if (gradient.alphaKeys.size() > 1 && ImGui::SmallButton("Remove"))
                removeAlpha = i;
            ImGui::PopID();
        }
        if (removeAlpha >= 0)
        {
            gradient.alphaKeys.erase(gradient.alphaKeys.begin() + removeAlpha);
            changed = true;
        }
        if (ImGui::SmallButton("Add Alpha Key"))
        {
            gradient.alphaKeys.push_back({1.0f, 1.0f});
            changed = true;
        }
        if (changed)
        {
            for (auto& key : gradient.colorKeys)
                key.time = std::clamp(key.time, 0.0f, 1.0f);
            for (auto& key : gradient.alphaKeys)
            {
                key.time = std::clamp(key.time, 0.0f, 1.0f);
                key.alpha = std::clamp(key.alpha, 0.0f, 1.0f);
            }
            gradient.SortKeys();
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
    return changed;
}

bool DrawColorParameter(const char* label, Particles::ColorParameter& parameter)
{
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    changed |= DrawEnum("Mode", parameter.mode, "Color\0Random Between Colors\0Gradient\0Random Between Gradients\0");
    switch (parameter.mode)
    {
        case Particles::ColorMode::Color:
            changed |= ImGui::ColorEdit4("Color", &parameter.color.x);
            break;
        case Particles::ColorMode::RandomBetweenColors:
            changed |= ImGui::ColorEdit4("Minimum", &parameter.colorMin.x);
            changed |= ImGui::ColorEdit4("Maximum", &parameter.colorMax.x);
            break;
        case Particles::ColorMode::Gradient:
            changed |= DrawGradient("Gradient", parameter.gradientMax);
            break;
        case Particles::ColorMode::RandomBetweenGradients:
            changed |= DrawGradient("Minimum", parameter.gradientMin);
            changed |= DrawGradient("Maximum", parameter.gradientMax);
            break;
    }
    ImGui::PopID();
    return changed;
}

bool DrawModuleHeader(const char* label, bool& enabled)
{
    const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    if (open)
        ImGui::Checkbox((std::string("Enabled##") + label).c_str(), &enabled);
    return open;
}
} // namespace

class ParticleSystemInspector : public Inspector<ParticleSystem>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        (void)editor;
        bool changed = false;

        DrawPlayback();

        if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("Main");
            changed |= DrawMain();
            ImGui::PopID();
        }

        auto& emission = target->GetEmissionModule();
        const bool emissionWasEnabled = emission.enabled;
        if (DrawModuleHeader("Emission", emission.enabled))
        {
            ImGui::PushID("Emission");
            changed |= DrawEmission(emission);
            ImGui::PopID();
        }
        changed |= emissionWasEnabled != emission.enabled;

        auto& shape = target->GetShapeModule();
        const bool shapeWasEnabled = shape.enabled;
        if (DrawModuleHeader("Shape", shape.enabled))
        {
            ImGui::PushID("Shape");
            changed |= DrawShape(shape);
            ImGui::PopID();
        }
        changed |= shapeWasEnabled != shape.enabled;

        auto& velocity = target->GetVelocityOverLifetimeModule();
        const bool velocityWasEnabled = velocity.enabled;
        if (DrawModuleHeader("Velocity over Lifetime", velocity.enabled))
        {
            ImGui::PushID("VelocityOverLifetime");
            changed |= DrawVectorParameter("Velocity", velocity.velocity);
            changed |= DrawScalarParameter("Speed Multiplier", velocity.speedMultiplier);
            changed |= ImGui::DragFloat("Inherit Emitter Velocity", &velocity.inheritEmitterVelocity, 0.01f);
            ImGui::PopID();
        }
        changed |= velocityWasEnabled != velocity.enabled;

        auto& limit = target->GetLimitVelocityModule();
        const bool limitWasEnabled = limit.enabled;
        if (DrawModuleHeader("Limit Velocity", limit.enabled))
        {
            ImGui::PushID("LimitVelocity");
            changed |= DrawScalarParameter("Speed Limit", limit.speedLimit);
            changed |= DrawScalarParameter("Drag", limit.drag);
            changed |= ImGui::DragFloat("Dampen", &limit.dampen, 0.01f, 0.0f, 1.0f);
            ImGui::PopID();
        }
        changed |= limitWasEnabled != limit.enabled;

        auto& color = target->GetColorOverLifetimeModule();
        const bool colorWasEnabled = color.enabled;
        if (DrawModuleHeader("Color over Lifetime", color.enabled))
        {
            ImGui::PushID("ColorOverLifetime");
            changed |= DrawColorParameter("Color", color.color);
            ImGui::PopID();
        }
        changed |= colorWasEnabled != color.enabled;

        auto& size = target->GetSizeOverLifetimeModule();
        const bool sizeWasEnabled = size.enabled;
        if (DrawModuleHeader("Size over Lifetime", size.enabled))
        {
            ImGui::PushID("SizeOverLifetime");
            changed |= DrawVectorParameter("Size", size.size);
            ImGui::PopID();
        }
        changed |= sizeWasEnabled != size.enabled;

        auto& rotation = target->GetRotationOverLifetimeModule();
        const bool rotationWasEnabled = rotation.enabled;
        if (DrawModuleHeader("Rotation over Lifetime", rotation.enabled))
        {
            ImGui::PushID("RotationOverLifetime");
            changed |= DrawVectorParameter("Angular Velocity (Degrees)", rotation.angularVelocity);
            ImGui::PopID();
        }
        changed |= rotationWasEnabled != rotation.enabled;

        auto& noise = target->GetNoiseModule();
        const bool noiseWasEnabled = noise.enabled;
        if (DrawModuleHeader("Noise", noise.enabled))
        {
            ImGui::PushID("Noise");
            changed |= DrawNoise(noise);
            ImGui::PopID();
        }
        changed |= noiseWasEnabled != noise.enabled;

        auto& textureSheet = target->GetTextureSheetModule();
        const bool textureWasEnabled = textureSheet.enabled;
        if (DrawModuleHeader("Texture Sheet Animation", textureSheet.enabled))
        {
            ImGui::PushID("TextureSheet");
            changed |= DrawTextureSheet(textureSheet);
            ImGui::PopID();
        }
        changed |= textureWasEnabled != textureSheet.enabled;

        if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("Renderer");
            changed |= DrawRenderer();
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Performance"))
        {
            ImGui::PushID("Performance");
            changed |= DrawPerformance();
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Scene Preview"))
        {
            ImGui::PushID("ScenePreview");
            changed |= DrawPreview();
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
            DrawDiagnostics();

        if (changed)
            target->NotifySettingsChanged();
    }

private:
    void DrawPlayback()
    {
        if (ImGui::Button("Play"))
            target->Play();
        ImGui::SameLine();
        if (ImGui::Button("Pause"))
            target->Pause();
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            target->Stop();
        ImGui::SameLine();
        if (ImGui::Button("Restart"))
            target->Restart();

        const char* state = "Stopped";
        if (target->GetPlaybackState() == Particles::PlaybackState::Playing)
            state = "Playing";
        else if (target->GetPlaybackState() == Particles::PlaybackState::Paused)
            state = "Paused";
        ImGui::Text("%s  |  Time %.2fs", state, target->GetSimulationTime());
    }

    bool DrawMain()
    {
        auto& main = target->GetMainModule();
        bool changed = false;
        changed |= ImGui::DragFloat("Duration", &main.duration, 0.05f, 0.0f);
        changed |= ImGui::Checkbox("Looping", &main.looping);
        changed |= ImGui::Checkbox("Prewarm", &main.prewarm);
        changed |= ImGui::Checkbox("Play on Awake", &main.playOnAwake);
        changed |= DrawScalarParameter("Start Delay", main.startDelay);
        changed |= DrawScalarParameter("Start Lifetime", main.startLifetime);
        changed |= DrawScalarParameter("Start Speed", main.startSpeed);
        changed |= DrawVectorParameter("Start Size", main.startSize);
        changed |= DrawVectorParameter("Start Rotation (Degrees)", main.startRotation);
        changed |= DrawColorParameter("Start Color", main.startColor);
        changed |= DrawScalarParameter("Gravity Modifier", main.gravityModifier);
        changed |= DrawEnum("Simulation Space", main.simulationSpace, "World\0Local\0");
        changed |= DrawEnum("Culling Mode", main.cullingMode, "Always Simulate\0Pause When Culled\0");
        changed |= ImGui::DragFloat("Simulation Speed", &main.simulationSpeed, 0.01f, 0.0f);
        changed |= ImGui::Checkbox("Fixed Time Step", &main.fixedTimeStep);
        if (main.fixedTimeStep)
            changed |= ImGui::DragFloat("Fixed Delta Time", &main.fixedDeltaTime, 0.0001f, 0.0001f);
        changed |= ImGui::DragInt("Max Particles", &main.maxParticles, 1.0f, 1);
        changed |= ImGui::Checkbox("Automatic Seed", &main.automaticSeed);
        if (!main.automaticSeed)
            changed |= ImGui::InputScalar("Seed", ImGuiDataType_U32, &main.seed);
        changed |= ImGui::Checkbox("Automatic Bounds", &main.automaticBounds);
        if (!main.automaticBounds)
        {
            changed |= ImGui::DragFloat3("Bounds Minimum", &main.manualBounds.min.x, 0.05f);
            changed |= ImGui::DragFloat3("Bounds Maximum", &main.manualBounds.max.x, 0.05f);
        }
        return changed;
    }

    bool DrawEmission(Particles::EmissionModule& emission)
    {
        bool changed = false;
        changed |= DrawScalarParameter("Rate over Time", emission.rateOverTime);
        changed |= DrawScalarParameter("Rate over Distance", emission.rateOverDistance);
        int removeIndex = -1;
        for (int i = 0; i < static_cast<int>(emission.bursts.size()); ++i)
        {
            ImGui::PushID(i);
            auto& burst = emission.bursts[i];
            if (ImGui::TreeNode("Burst"))
            {
                changed |= ImGui::DragFloat("Time", &burst.time, 0.01f, 0.0f);
                changed |= ImGui::DragInt("Count", &burst.count, 1.0f, 0);
                changed |= ImGui::DragInt("Cycles", &burst.cycles, 1.0f, 1);
                changed |= ImGui::DragFloat("Interval", &burst.interval, 0.01f, 0.0f);
                changed |= ImGui::DragFloat("Probability", &burst.probability, 0.01f, 0.0f, 1.0f);
                if (ImGui::SmallButton("Remove Burst"))
                    removeIndex = i;
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if (removeIndex >= 0)
        {
            emission.bursts.erase(emission.bursts.begin() + removeIndex);
            changed = true;
        }
        if (ImGui::Button("Add Burst"))
        {
            emission.bursts.emplace_back();
            changed = true;
        }
        return changed;
    }

    bool DrawShape(Particles::ShapeModule& shape)
    {
        bool changed = false;
        changed |= DrawEnum("Shape", shape.shape, "Point\0Disk\0Rectangle\0Box\0Sphere\0Hemisphere\0Cone\0");
        changed |= DrawEnum("Sampling", shape.sampling, "Random\0Poisson Disk\0");
        changed |= DrawEnum("Direction", shape.direction, "Axis\0Outward\0Random\0");
        if (shape.direction == Particles::ShapeDirection::Axis)
            changed |= ImGui::DragFloat3("Axis", &shape.axis.x, 0.01f);
        changed |= ImGui::DragFloat3("Rotation", &shape.rotation.x, 0.1f);
        if (shape.shape == Particles::ShapeType::Disk || shape.shape == Particles::ShapeType::Sphere ||
            shape.shape == Particles::ShapeType::Hemisphere || shape.shape == Particles::ShapeType::Cone)
            changed |= ImGui::DragFloat("Radius", &shape.radius, 0.01f, 0.0f);
        if (shape.shape == Particles::ShapeType::Rectangle || shape.shape == Particles::ShapeType::Box)
            changed |= ImGui::DragFloat3("Size", &shape.size.x, 0.01f, 0.0f);
        if (shape.shape == Particles::ShapeType::Cone)
        {
            changed |= ImGui::DragFloat("Cone Angle", &shape.coneAngle, 0.1f, 0.0f, 89.9f);
            changed |= ImGui::DragFloat("Cone Length", &shape.coneLength, 0.01f, 0.0f);
        }
        if (shape.shape != Particles::ShapeType::Point && shape.shape != Particles::ShapeType::Rectangle)
            changed |= ImGui::Checkbox("Emit from Shell", &shape.shell);
        if (shape.sampling == Particles::ShapeSampling::PoissonDisk)
            changed |= ImGui::DragFloat("Poisson Minimum Distance", &shape.poissonMinDistance, 0.01f, 0.0f);
        return changed;
    }

    bool DrawNoise(Particles::NoiseModule& noise)
    {
        bool changed = false;
        changed |= DrawVectorParameter("Strength", noise.strength);
        changed |= ImGui::DragFloat("Frequency", &noise.frequency, 0.01f, 0.0f);
        changed |= ImGui::DragFloat3("Scroll Velocity", &noise.scrollVelocity.x, 0.01f);
        changed |= ImGui::DragInt("Octaves", &noise.octaves, 1.0f, 1, 8);
        changed |= ImGui::DragFloat("Octave Strength Multiplier", &noise.octaveStrengthMultiplier, 0.01f);
        changed |= ImGui::DragFloat("Octave Frequency Multiplier", &noise.octaveFrequencyMultiplier, 0.01f, 0.0f);
        changed |= ImGui::DragFloat("Damping", &noise.damping, 0.01f, 0.0f);
        changed |= DrawEnum("Space", noise.space, "World\0Local\0");
        changed |= ImGui::DragFloat("Position Influence", &noise.positionInfluence, 0.01f);
        changed |= ImGui::DragFloat("Rotation Influence", &noise.rotationInfluence, 0.01f);
        changed |= ImGui::DragFloat("Size Influence", &noise.sizeInfluence, 0.01f);
        changed |= ImGui::InputScalar("Seed Offset", ImGuiDataType_U32, &noise.seedOffset);
        return changed;
    }

    bool DrawTextureSheet(Particles::TextureSheetModule& textureSheet)
    {
        bool changed = false;
        changed |= ImGui::DragInt("Columns", &textureSheet.columns, 1.0f, 1);
        changed |= ImGui::DragInt("Rows", &textureSheet.rows, 1.0f, 1);
        changed |= DrawScalarParameter("Frame over Lifetime", textureSheet.frameOverLifetime);
        changed |= ImGui::DragFloat("Cycles", &textureSheet.cycles, 0.01f, 0.0f);
        changed |= ImGui::Checkbox("Random Start Frame", &textureSheet.randomStartFrame);
        return changed;
    }

    bool DrawRenderer()
    {
        auto& renderer = target->GetRendererModule();
        bool changed = false;
        changed |= DrawEnum("Mode", renderer.mode, "Billboard\0Mesh\0");
        if (renderer.mode == Particles::RendererMode::Billboard)
        {
            changed |= DrawEnum("Billboard Mode", renderer.billboardMode, "Camera Facing\0Vertical\0Horizontal\0Stretched\0");
            changed |= ImGui::DragFloat2("Pivot", &renderer.pivot.x, 0.01f);
            if (renderer.billboardMode == Particles::BillboardMode::Stretched)
            {
                changed |= ImGui::DragFloat("Stretch Scale", &renderer.stretchScale, 0.01f, 0.0f);
                changed |= ImGui::DragFloat("Velocity Scale", &renderer.velocityScale, 0.01f, 0.0f);
            }
        }
        else
        {
            Mesh* mesh = renderer.mesh.Get();
            if (EditorGUI::ObjectField("Mesh", mesh))
            {
                renderer.mesh = mesh;
                changed = true;
            }
        }
        Material* material = renderer.material.Get();
        if (EditorGUI::ObjectField("Particle Material", material))
        {
            renderer.material = material;
            changed = true;
        }
        changed |= DrawEnum("Sort Mode", renderer.sortMode, "None\0Distance\0Youngest First\0Oldest First\0");
        changed |= ImGui::DragFloat("Sort Bias", &renderer.sortBias, 0.01f);
        changed |= ImGui::DragFloat("Minimum Screen Size", &renderer.minScreenSize, 0.001f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Maximum Screen Size", &renderer.maxScreenSize, 0.001f, 0.0f, 1.0f);

        if (material != nullptr && material->GetShader().Get() != ShaderLibrary::GetShader(Shaders::Particle))
            ImGui::TextColored(
                {1.0f, 0.7f, 0.2f, 1.0f},
                "Material must use the Particle shader; defaults will be used."
            );
        return changed;
    }

    bool DrawPerformance()
    {
        auto& performance = target->GetPerformanceSettings();
        bool changed = ImGui::Checkbox("Multithreaded Simulation (Auto)", &performance.multithreadedSimulation);
        ImGui::TextDisabled("Jobs are scheduled at 512 active particles or more.");
        return changed;
    }

    bool DrawPreview()
    {
        auto& preview = target->GetPreviewSettings();
        bool changed = false;
        changed |= ImGui::Checkbox("Show Shape Samples", &preview.showShape);
        if (preview.showShape)
        {
            changed |= ImGui::DragInt("Shape Sample Count", &preview.shapeSampleCount, 1.0f, 1, 256);
            changed |= ImGui::DragFloat("Sample Marker Size", &preview.sampleMarkerSize, 0.001f, 0.001f);
        }
        changed |= ImGui::Checkbox("Show Bounds", &preview.showBounds);
        return changed;
    }

    void DrawDiagnostics()
    {
        const auto& stats = target->GetSimulationStats();
        ImGui::Text("Active: %d / %d", stats.activeParticles, target->GetParticleCount());
        ImGui::Text("Emitted: %d", stats.emittedThisFrame);
        ImGui::Text("Dropped: %d", stats.droppedParticles);
        ImGui::Text("Jobs: %d", stats.scheduledJobs);
        ImGui::Text("Simulation: %.3f ms", stats.simulationMilliseconds);
        ImGui::Text("Sorting: %.3f ms", stats.sortingMilliseconds);
        ImGui::Text("Upload: %.3f ms", stats.uploadMilliseconds);
        ImGui::Text("Visible: %s", stats.visible ? "Yes" : "No");
        if (stats.capacitySaturated)
            ImGui::TextColored({1.0f, 0.35f, 0.2f, 1.0f}, "Capacity reached; increase Max Particles.");
        const auto& main = target->GetMainModule();
        if (!main.automaticBounds && glm::any(glm::greaterThan(main.manualBounds.min, main.manualBounds.max)))
            ImGui::TextColored({1.0f, 0.35f, 0.2f, 1.0f}, "Manual bounds minimum exceeds maximum.");
        const auto& renderer = target->GetRendererModule();
        if (renderer.mode == Particles::RendererMode::Mesh && renderer.mesh == nullptr)
            ImGui::TextColored({1.0f, 0.7f, 0.2f, 1.0f}, "Mesh mode has no mesh; the internal sphere will be used.");
    }

    static const char _register;
};

const char ParticleSystemInspector::_register = InspectorRegistry::Register<ParticleSystemInspector, ParticleSystem>();

} // namespace Editor
