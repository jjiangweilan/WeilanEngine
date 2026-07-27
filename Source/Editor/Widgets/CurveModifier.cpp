#include "CurveModifier.hpp"

#include "Engine/Runtime/Object/Component/ParticleSystem.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/ThirdParty/imgui/implot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace Editor
{
namespace
{
constexpr int CurveSampleCount = 129;
constexpr float MinimumKeySeparation = 0.0001f;

int InsertKey(Particles::Curve& curve, float time, float value)
{
    Particles::CurveKey key;
    key.time = std::clamp(time, 0.0f, 1.0f);
    key.value = value;
    key.interpolation = Particles::CurveInterpolation::Linear;

    const auto insertion = std::lower_bound(
        curve.keys.begin(),
        curve.keys.end(),
        key.time,
        [](const Particles::CurveKey& existing, float candidateTime) { return existing.time < candidateTime; }
    );
    const int index = static_cast<int>(std::distance(curve.keys.begin(), insertion));
    if (insertion != curve.keys.end() && std::abs(insertion->time - key.time) < MinimumKeySeparation)
        return index;
    if (insertion != curve.keys.begin() &&
        std::abs((insertion - 1)->time - key.time) < MinimumKeySeparation)
        return index - 1;
    curve.keys.insert(insertion, key);
    return index;
}

float FindNewKeyTime(const Particles::Curve& curve)
{
    if (curve.keys.empty())
        return 0.5f;

    float bestStart = 0.0f;
    float bestEnd = curve.keys.front().time;
    for (size_t i = 1; i < curve.keys.size(); ++i)
    {
        const float gapStart = curve.keys[i - 1].time;
        const float gapEnd = curve.keys[i].time;
        if (gapEnd - gapStart > bestEnd - bestStart)
        {
            bestStart = gapStart;
            bestEnd = gapEnd;
        }
    }
    if (1.0f - curve.keys.back().time > bestEnd - bestStart)
    {
        bestStart = curve.keys.back().time;
        bestEnd = 1.0f;
    }
    return (bestStart + bestEnd) * 0.5f;
}

void CalculateValueRange(
    const Particles::Curve& curve,
    std::array<double, CurveSampleCount>& times,
    std::array<double, CurveSampleCount>& values,
    double& minimum,
    double& maximum
)
{
    minimum = std::numeric_limits<double>::max();
    maximum = std::numeric_limits<double>::lowest();
    for (int i = 0; i < CurveSampleCount; ++i)
    {
        const float normalizedTime = static_cast<float>(i) / static_cast<float>(CurveSampleCount - 1);
        times[i] = normalizedTime;
        values[i] = curve.Evaluate(normalizedTime);
        minimum = std::min(minimum, values[i]);
        maximum = std::max(maximum, values[i]);
    }

    for (const Particles::CurveKey& key : curve.keys)
    {
        minimum = std::min(minimum, static_cast<double>(key.value));
        maximum = std::max(maximum, static_cast<double>(key.value));
    }

    if (!std::isfinite(minimum) || !std::isfinite(maximum))
    {
        minimum = 0.0;
        maximum = 1.0;
    }
    const double range = maximum - minimum;
    const double padding = range > 0.0001 ? range * 0.15 : std::max(std::abs(maximum) * 0.1, 0.5);
    minimum -= padding;
    maximum += padding;
}

bool DrawTangentHandle(
    int id,
    Particles::CurveKey& key,
    float adjacentTime,
    bool incoming,
    const ImVec4& color
)
{
    const float interval = std::abs(key.time - adjacentTime);
    if (interval <= MinimumKeySeparation)
        return false;
    const float handleDistance = std::max(interval * 0.3f, 0.03f);
    double handleTime = incoming ? key.time - handleDistance : key.time + handleDistance;
    handleTime = std::clamp(handleTime, static_cast<double>(std::min(key.time, adjacentTime)),
                            static_cast<double>(std::max(key.time, adjacentTime)));
    const float tangent = incoming ? key.inTangent : key.outTangent;
    double handleValue = incoming ? key.value - tangent * (key.time - static_cast<float>(handleTime))
                                  : key.value + tangent * (static_cast<float>(handleTime) - key.time);

    const double lineTimes[2] = {key.time, handleTime};
    const double lineValues[2] = {key.value, handleValue};
    ImPlot::SetNextLineStyle(color, 1.0f);
    ImPlot::PlotLine(incoming ? "##IncomingTangent" : "##OutgoingTangent", lineTimes, lineValues, 2);

    bool clicked = false;
    bool hovered = false;
    const bool moved = ImPlot::DragPoint(id, &handleTime, &handleValue, color, 4.0f, ImPlotDragToolFlags_NoFit,
                                         &clicked, &hovered);
    if (!moved)
        return false;

    if (incoming)
    {
        handleTime = std::clamp(handleTime, static_cast<double>(adjacentTime),
                                static_cast<double>(key.time - MinimumKeySeparation));
        key.inTangent = (key.value - static_cast<float>(handleValue)) /
                        (key.time - static_cast<float>(handleTime));
    }
    else
    {
        handleTime = std::clamp(handleTime, static_cast<double>(key.time + MinimumKeySeparation),
                                static_cast<double>(adjacentTime));
        key.outTangent = (static_cast<float>(handleValue) - key.value) /
                         (static_cast<float>(handleTime) - key.time);
    }
    return true;
}
} // namespace

bool CurveModifier::Draw(const char* label, Particles::Curve& curve)
{
    bool changed = false;
    ImGui::PushID(label);
    const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen);
    if (!open)
    {
        ImGui::PopID();
        return false;
    }

    ImGuiStorage* state = ImGui::GetStateStorage();
    const ImGuiID selectedKeyState = ImGui::GetID("SelectedKey");
    int selectedKey = state->GetInt(selectedKeyState, curve.keys.empty() ? -1 : 0);
    if (curve.keys.empty())
        selectedKey = -1;
    else
        selectedKey = std::clamp(selectedKey, 0, static_cast<int>(curve.keys.size()) - 1);

    if (ImGui::SmallButton("Add Key"))
    {
        const float time = FindNewKeyTime(curve);
        selectedKey = InsertKey(curve, time, curve.Evaluate(time));
        changed = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(curve.keys.size() <= 1 || selectedKey < 0);
    const bool deleteButtonPressed = ImGui::SmallButton("Delete Key");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset 0-1"))
    {
        curve.keys = {{0.0f, 0.0f}, {1.0f, 1.0f}};
        selectedKey = 0;
        changed = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Replace the curve with a linear 0 to 1 ramp.");

    std::array<double, CurveSampleCount> sampledTimes;
    std::array<double, CurveSampleCount> sampledValues;
    double minimumValue = 0.0;
    double maximumValue = 1.0;
    CalculateValueRange(curve, sampledTimes, sampledValues, minimumValue, maximumValue);

    int pendingDelete = deleteButtonPressed ? selectedKey : -1;
    bool plotHovered = false;
    const ImPlotFlags plotFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                                  ImPlotFlags_NoBoxSelect;
    ImPlot::SetNextAxesLimits(0.0, 1.0, minimumValue, maximumValue, ImPlotCond_Always);
    if (ImPlot::BeginPlot("##VisualCurve", ImVec2(-1.0f, 190.0f), plotFlags))
    {
        const ImPlotAxisFlags axisFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoMenus |
                                          ImPlotAxisFlags_NoSideSwitch;
        ImPlot::SetupAxes("Normalized Lifetime", "Value", axisFlags, axisFlags);

        ImPlot::SetNextLineStyle(ImVec4(0.2f, 0.75f, 1.0f, 1.0f), 2.0f);
        ImPlot::PlotLine("##Curve", sampledTimes.data(), sampledValues.data(), CurveSampleCount);

        bool anyPointHovered = false;
        for (int i = 0; i < static_cast<int>(curve.keys.size()); ++i)
        {
            Particles::CurveKey& key = curve.keys[i];
            double time = key.time;
            double value = key.value;
            bool clicked = false;
            bool hovered = false;
            const ImVec4 color = i == selectedKey ? ImVec4(1.0f, 0.55f, 0.1f, 1.0f)
                                                  : ImVec4(0.2f, 0.75f, 1.0f, 1.0f);
            if (ImPlot::DragPoint(i + 1, &time, &value, color, i == selectedKey ? 6.0f : 5.0f,
                                  ImPlotDragToolFlags_NoFit, &clicked, &hovered))
            {
                const float minimumTime = i > 0 ? curve.keys[i - 1].time + MinimumKeySeparation : 0.0f;
                const float maximumTime = i + 1 < static_cast<int>(curve.keys.size())
                                                  ? curve.keys[i + 1].time - MinimumKeySeparation
                                                  : 1.0f;
                key.time = std::clamp(static_cast<float>(time), minimumTime, maximumTime);
                key.value = static_cast<float>(value);
                selectedKey = i;
                changed = true;
            }
            if (clicked)
                selectedKey = i;
            if (hovered)
            {
                anyPointHovered = true;
                ImGui::SetTooltip("Key %d\nTime %.3f\nValue %.3f", i, key.time, key.value);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && curve.keys.size() > 1)
                    pendingDelete = i;
            }
        }

        if (selectedKey >= 0 && selectedKey < static_cast<int>(curve.keys.size()))
        {
            Particles::CurveKey& selected = curve.keys[selectedKey];
            const ImVec4 tangentColor(1.0f, 0.75f, 0.25f, 1.0f);
            if (selectedKey > 0 &&
                curve.keys[selectedKey - 1].interpolation == Particles::CurveInterpolation::Cubic)
                changed |= DrawTangentHandle(10000 + selectedKey * 2, selected, curve.keys[selectedKey - 1].time,
                                             true, tangentColor);
            if (selectedKey + 1 < static_cast<int>(curve.keys.size()) &&
                selected.interpolation == Particles::CurveInterpolation::Cubic)
                changed |= DrawTangentHandle(10001 + selectedKey * 2, selected, curve.keys[selectedKey + 1].time,
                                             false, tangentColor);
        }

        plotHovered = ImPlot::IsPlotHovered();
        if (plotHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !anyPointHovered)
        {
            const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
            selectedKey = InsertKey(curve, static_cast<float>(mouse.x), static_cast<float>(mouse.y));
            changed = true;
        }
        ImPlot::EndPlot();
    }

    if (plotHovered && selectedKey >= 0 && curve.keys.size() > 1 && ImGui::IsKeyPressed(ImGuiKey_Delete))
        pendingDelete = selectedKey;

    if (pendingDelete >= 0 && pendingDelete < static_cast<int>(curve.keys.size()) && curve.keys.size() > 1)
    {
        curve.keys.erase(curve.keys.begin() + pendingDelete);
        selectedKey = std::min(pendingDelete, static_cast<int>(curve.keys.size()) - 1);
        changed = true;
    }

    if (selectedKey >= 0 && selectedKey < static_cast<int>(curve.keys.size()))
    {
        Particles::CurveKey& key = curve.keys[selectedKey];
        ImGui::SeparatorText("Selected Key");
        ImGui::PushID(selectedKey);
        float editedTime = key.time;
        if (ImGui::DragFloat("Time", &editedTime, 0.002f, 0.0f, 1.0f, "%.3f"))
        {
            const float minimumTime = selectedKey > 0 ? curve.keys[selectedKey - 1].time + MinimumKeySeparation
                                                       : 0.0f;
            const float maximumTime = selectedKey + 1 < static_cast<int>(curve.keys.size())
                                              ? curve.keys[selectedKey + 1].time - MinimumKeySeparation
                                              : 1.0f;
            key.time = std::clamp(editedTime, minimumTime, maximumTime);
            changed = true;
        }
        changed |= ImGui::DragFloat("Value", &key.value, 0.01f);

        int interpolation = static_cast<int>(key.interpolation);
        if (ImGui::Combo("Interpolation", &interpolation, "Constant\0Linear\0Cubic\0"))
        {
            key.interpolation = static_cast<Particles::CurveInterpolation>(interpolation);
            changed = true;
        }
        const bool incomingCubic = selectedKey > 0 &&
                                   curve.keys[selectedKey - 1].interpolation == Particles::CurveInterpolation::Cubic;
        if (incomingCubic)
            changed |= ImGui::DragFloat("In Tangent", &key.inTangent, 0.01f);
        if (key.interpolation == Particles::CurveInterpolation::Cubic &&
            selectedKey + 1 < static_cast<int>(curve.keys.size()))
            changed |= ImGui::DragFloat("Out Tangent", &key.outTangent, 0.01f);
        ImGui::PopID();
    }
    else
    {
        ImGui::TextDisabled("Double-click the graph or press Add Key to create a key.");
    }

    state->SetInt(selectedKeyState, selectedKey);
    ImGui::TextDisabled("Drag keys to edit. Double-click to add. Right-click or Delete to remove.");
    ImGui::TreePop();
    ImGui::PopID();
    return changed;
}
} // namespace Editor
