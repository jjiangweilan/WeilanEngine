#include "ParticleSystem.hpp"

#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Library/Random.hpp"
#include "Engine/Library/Serialization/Serializer.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <numeric>

DEFINE_COMPONENT(ParticleSystem, "78E33F89-76E6-4B90-831F-490EB6C9F8D1")

namespace
{
constexpr float TwoPi = 6.28318530717958647692f;
constexpr float MinimumLifetime = 0.0001f;

template <class Enum>
void SerializeEnum(Serializer* s, std::string_view name, Enum value)
{
    s->Serialize(name, static_cast<int>(value));
}

template <class Enum>
void DeserializeEnum(Serializer* s, std::string_view name, Enum& value)
{
    int serializedValue = static_cast<int>(value);
    s->Deserialize(name, serializedValue);
    value = static_cast<Enum>(serializedValue);
}

float SmoothStep(float value)
{
    return value * value * (3.0f - 2.0f * value);
}

uint32_t HashCoordinates(const int3& coordinates, uint32_t seed)
{
    uint32_t hash = seed ^ 0x9E3779B9u;
    hash ^= static_cast<uint32_t>(coordinates.x) * 0x85EBCA6Bu;
    hash = (hash << 13u) | (hash >> 19u);
    hash ^= static_cast<uint32_t>(coordinates.y) * 0xC2B2AE35u;
    hash = (hash << 11u) | (hash >> 21u);
    hash ^= static_cast<uint32_t>(coordinates.z) * 0x27D4EB2Fu;
    hash ^= hash >> 16u;
    hash *= 0x7FEB352Du;
    hash ^= hash >> 15u;
    return hash;
}

float LatticeNoise(const int3& coordinates, uint32_t seed)
{
    return static_cast<float>(HashCoordinates(coordinates, seed) & 0x00FFFFFFu) / 8388607.5f - 1.0f;
}

float ValueNoise(const float3& position, uint32_t seed)
{
    const int3 cell = int3(glm::floor(position));
    const float3 fraction = glm::fract(position);
    const float3 blend = float3(SmoothStep(fraction.x), SmoothStep(fraction.y), SmoothStep(fraction.z));

    float samples[2][2][2];
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                samples[x][y][z] = LatticeNoise(cell + int3(x, y, z), seed);

    const float z0y0 = glm::mix(samples[0][0][0], samples[1][0][0], blend.x);
    const float z0y1 = glm::mix(samples[0][1][0], samples[1][1][0], blend.x);
    const float z1y0 = glm::mix(samples[0][0][1], samples[1][0][1], blend.x);
    const float z1y1 = glm::mix(samples[0][1][1], samples[1][1][1], blend.x);
    const float z0 = glm::mix(z0y0, z0y1, blend.y);
    const float z1 = glm::mix(z1y0, z1y1, blend.y);
    return glm::mix(z0, z1, blend.z);
}

float3 NoisePotential(const float3& position, uint32_t seed)
{
    return {
        ValueNoise(position, seed ^ 0x68BC21EBu),
        ValueNoise(position + float3(19.1f, 7.3f, 3.7f), seed ^ 0x02E5BE93u),
        ValueNoise(position + float3(5.7f, 23.9f, 11.1f), seed ^ 0x967A889Bu)
    };
}

float3 CurlNoiseOctave(const float3& position, uint32_t seed)
{
    constexpr float epsilon = 0.01f;
    const float3 dx(epsilon, 0.0f, 0.0f);
    const float3 dy(0.0f, epsilon, 0.0f);
    const float3 dz(0.0f, 0.0f, epsilon);

    const float3 derivativeX = (NoisePotential(position + dx, seed) - NoisePotential(position - dx, seed)) /
                               (2.0f * epsilon);
    const float3 derivativeY = (NoisePotential(position + dy, seed) - NoisePotential(position - dy, seed)) /
                               (2.0f * epsilon);
    const float3 derivativeZ = (NoisePotential(position + dz, seed) - NoisePotential(position - dz, seed)) /
                               (2.0f * epsilon);

    return {
        derivativeY.z - derivativeZ.y,
        derivativeZ.x - derivativeX.z,
        derivativeX.y - derivativeY.x
    };
}

float4 LerpColor(const float4& a, const float4& b, float t)
{
    return glm::mix(a, b, glm::clamp(t, 0.0f, 1.0f));
}

glm::mat3 NormalizeBasis(const glm::mat3& basis)
{
    glm::mat3 result(1.0f);
    for (int axis = 0; axis < 3; ++axis)
    {
        const float axisLength = glm::length(basis[axis]);
        if (axisLength > MinimumLifetime)
            result[axis] = basis[axis] / axisLength;
    }
    return result;
}
} // namespace

namespace Particles
{
void CurveKey::Serialize(Serializer* s) const
{
    SERIALIZE(s, time);
    SERIALIZE(s, value);
    SERIALIZE(s, inTangent);
    SERIALIZE(s, outTangent);
    SerializeEnum(s, "interpolation", interpolation);
}

void CurveKey::Deserialize(Serializer* s)
{
    DESERIALIZE(s, time);
    DESERIALIZE(s, value);
    DESERIALIZE(s, inTangent);
    DESERIALIZE(s, outTangent);
    DeserializeEnum(s, "interpolation", interpolation);
}

Curve::Curve(float value)
{
    keys = {{0.0f, value}, {1.0f, value}};
}

float Curve::Evaluate(float normalizedTime) const
{
    if (keys.empty())
        return 0.0f;
    if (keys.size() == 1 || normalizedTime <= keys.front().time)
        return keys.front().value;
    if (normalizedTime >= keys.back().time)
        return keys.back().value;

    const auto right = std::upper_bound(
        keys.begin(),
        keys.end(),
        normalizedTime,
        [](float time, const CurveKey& key) { return time < key.time; }
    );
    const CurveKey& b = *right;
    const CurveKey& a = *(right - 1);
    const float interval = b.time - a.time;
    if (a.interpolation == CurveInterpolation::Constant || interval <= 0.0f)
        return a.value;

    const float t = glm::clamp((normalizedTime - a.time) / interval, 0.0f, 1.0f);
    if (a.interpolation == CurveInterpolation::Linear)
        return glm::mix(a.value, b.value, t);

    const float t2 = t * t;
    const float t3 = t2 * t;
    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 = t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    const float h11 = t3 - t2;
    return h00 * a.value + h10 * a.outTangent * interval + h01 * b.value + h11 * b.inTangent * interval;
}

void Curve::SortKeys()
{
    std::stable_sort(keys.begin(), keys.end(), [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
}

void Curve::Serialize(Serializer* s) const
{
    SERIALIZE(s, keys);
}

void Curve::Deserialize(Serializer* s)
{
    DESERIALIZE(s, keys);
    SortKeys();
}

ScalarParameter::ScalarParameter(float value)
    : constant(value), constantMin(value), constantMax(value), curveMin(value), curveMax(value) {}

float ScalarParameter::Evaluate(float normalizedTime, float random) const
{
    switch (mode)
    {
        case ScalarMode::Constant: return constant;
        case ScalarMode::RandomBetweenConstants: return glm::mix(constantMin, constantMax, random);
        case ScalarMode::Curve: return curveMax.Evaluate(normalizedTime);
        case ScalarMode::RandomBetweenCurves:
            return glm::mix(curveMin.Evaluate(normalizedTime), curveMax.Evaluate(normalizedTime), random);
    }
    return constant;
}

void ScalarParameter::Serialize(Serializer* s) const
{
    SerializeEnum(s, "mode", mode);
    SERIALIZE(s, constant);
    SERIALIZE(s, constantMin);
    SERIALIZE(s, constantMax);
    SERIALIZE(s, curveMin);
    SERIALIZE(s, curveMax);
}

void ScalarParameter::Deserialize(Serializer* s)
{
    DeserializeEnum(s, "mode", mode);
    DESERIALIZE(s, constant);
    DESERIALIZE(s, constantMin);
    DESERIALIZE(s, constantMax);
    DESERIALIZE(s, curveMin);
    DESERIALIZE(s, curveMax);
}

VectorParameter::VectorParameter(float value)
    : x(value), y(value), z(value) {}

VectorParameter::VectorParameter(const float3& value)
    : separateAxes(true), x(value.x), y(value.y), z(value.z) {}

float3 VectorParameter::Evaluate(float normalizedTime, const float3& random) const
{
    const float xValue = x.Evaluate(normalizedTime, random.x);
    if (!separateAxes)
        return float3(xValue);
    return {xValue, y.Evaluate(normalizedTime, random.y), z.Evaluate(normalizedTime, random.z)};
}

void VectorParameter::Serialize(Serializer* s) const
{
    SERIALIZE(s, separateAxes);
    SERIALIZE(s, x);
    SERIALIZE(s, y);
    SERIALIZE(s, z);
}

void VectorParameter::Deserialize(Serializer* s)
{
    DESERIALIZE(s, separateAxes);
    DESERIALIZE(s, x);
    DESERIALIZE(s, y);
    DESERIALIZE(s, z);
}

void GradientColorKey::Serialize(Serializer* s) const
{
    SERIALIZE(s, time);
    SERIALIZE(s, color);
}

void GradientColorKey::Deserialize(Serializer* s)
{
    DESERIALIZE(s, time);
    DESERIALIZE(s, color);
}

void GradientAlphaKey::Serialize(Serializer* s) const
{
    SERIALIZE(s, time);
    SERIALIZE(s, alpha);
}

void GradientAlphaKey::Deserialize(Serializer* s)
{
    DESERIALIZE(s, time);
    DESERIALIZE(s, alpha);
}

Gradient::Gradient()
    : Gradient(float4(1.0f)) {}

Gradient::Gradient(const float4& color)
{
    colorKeys = {{0.0f, float3(color)}, {1.0f, float3(color)}};
    alphaKeys = {{0.0f, color.a}, {1.0f, color.a}};
}

float4 Gradient::Evaluate(float normalizedTime) const
{
    auto evaluateColor = [normalizedTime](const std::vector<GradientColorKey>& keys)
    {
        if (keys.empty())
            return float3(1.0f);
        if (keys.size() == 1 || normalizedTime <= keys.front().time)
            return keys.front().color;
        if (normalizedTime >= keys.back().time)
            return keys.back().color;
        const auto right = std::upper_bound(
            keys.begin(),
            keys.end(),
            normalizedTime,
            [](float time, const GradientColorKey& key) { return time < key.time; }
        );
        const GradientColorKey& b = *right;
        const GradientColorKey& a = *(right - 1);
        return glm::mix(a.color, b.color, (normalizedTime - a.time) / (b.time - a.time));
    };

    auto evaluateAlpha = [normalizedTime](const std::vector<GradientAlphaKey>& keys)
    {
        if (keys.empty())
            return 1.0f;
        if (keys.size() == 1 || normalizedTime <= keys.front().time)
            return keys.front().alpha;
        if (normalizedTime >= keys.back().time)
            return keys.back().alpha;
        const auto right = std::upper_bound(
            keys.begin(),
            keys.end(),
            normalizedTime,
            [](float time, const GradientAlphaKey& key) { return time < key.time; }
        );
        const GradientAlphaKey& b = *right;
        const GradientAlphaKey& a = *(right - 1);
        return glm::mix(a.alpha, b.alpha, (normalizedTime - a.time) / (b.time - a.time));
    };

    return float4(evaluateColor(colorKeys), evaluateAlpha(alphaKeys));
}

void Gradient::SortKeys()
{
    std::stable_sort(
        colorKeys.begin(),
        colorKeys.end(),
        [](const GradientColorKey& a, const GradientColorKey& b) { return a.time < b.time; }
    );
    std::stable_sort(
        alphaKeys.begin(),
        alphaKeys.end(),
        [](const GradientAlphaKey& a, const GradientAlphaKey& b) { return a.time < b.time; }
    );
}

void Gradient::Serialize(Serializer* s) const
{
    SERIALIZE(s, colorKeys);
    SERIALIZE(s, alphaKeys);
}

void Gradient::Deserialize(Serializer* s)
{
    DESERIALIZE(s, colorKeys);
    DESERIALIZE(s, alphaKeys);
    SortKeys();
}

float4 ColorParameter::Evaluate(float normalizedTime, float random) const
{
    switch (mode)
    {
        case ColorMode::Color: return color;
        case ColorMode::RandomBetweenColors: return LerpColor(colorMin, colorMax, random);
        case ColorMode::Gradient: return gradientMax.Evaluate(normalizedTime);
        case ColorMode::RandomBetweenGradients:
            return LerpColor(gradientMin.Evaluate(normalizedTime), gradientMax.Evaluate(normalizedTime), random);
    }
    return color;
}

void ColorParameter::Serialize(Serializer* s) const
{
    SerializeEnum(s, "mode", mode);
    SERIALIZE(s, color);
    SERIALIZE(s, colorMin);
    SERIALIZE(s, colorMax);
    SERIALIZE(s, gradientMin);
    SERIALIZE(s, gradientMax);
}

void ColorParameter::Deserialize(Serializer* s)
{
    DeserializeEnum(s, "mode", mode);
    DESERIALIZE(s, color);
    DESERIALIZE(s, colorMin);
    DESERIALIZE(s, colorMax);
    DESERIALIZE(s, gradientMin);
    DESERIALIZE(s, gradientMax);
}

void MainModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, duration);
    SERIALIZE(s, looping);
    SERIALIZE(s, prewarm);
    SERIALIZE(s, startDelay);
    SERIALIZE(s, startLifetime);
    SERIALIZE(s, startSpeed);
    SERIALIZE(s, startSize);
    SERIALIZE(s, startRotation);
    SERIALIZE(s, startColor);
    SERIALIZE(s, gravityModifier);
    SerializeEnum(s, "simulationSpace", simulationSpace);
    SerializeEnum(s, "cullingMode", cullingMode);
    SERIALIZE(s, simulationSpeed);
    SERIALIZE(s, fixedTimeStep);
    SERIALIZE(s, fixedDeltaTime);
    SERIALIZE(s, playOnAwake);
    SERIALIZE(s, maxParticles);
    SERIALIZE(s, automaticSeed);
    SERIALIZE(s, seed);
    SERIALIZE(s, automaticBounds);
    s->Serialize("manualBoundsMin", manualBounds.min);
    s->Serialize("manualBoundsMax", manualBounds.max);
}

void MainModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, duration);
    DESERIALIZE(s, looping);
    DESERIALIZE(s, prewarm);
    DESERIALIZE(s, startDelay);
    DESERIALIZE(s, startLifetime);
    DESERIALIZE(s, startSpeed);
    DESERIALIZE(s, startSize);
    DESERIALIZE(s, startRotation);
    DESERIALIZE(s, startColor);
    DESERIALIZE(s, gravityModifier);
    DeserializeEnum(s, "simulationSpace", simulationSpace);
    DeserializeEnum(s, "cullingMode", cullingMode);
    DESERIALIZE(s, simulationSpeed);
    DESERIALIZE(s, fixedTimeStep);
    DESERIALIZE(s, fixedDeltaTime);
    DESERIALIZE(s, playOnAwake);
    DESERIALIZE(s, maxParticles);
    DESERIALIZE(s, automaticSeed);
    DESERIALIZE(s, seed);
    DESERIALIZE(s, automaticBounds);
    s->Deserialize("manualBoundsMin", manualBounds.min);
    s->Deserialize("manualBoundsMax", manualBounds.max);
}

void Burst::Serialize(Serializer* s) const
{
    SERIALIZE(s, time);
    SERIALIZE(s, count);
    SERIALIZE(s, cycles);
    SERIALIZE(s, interval);
    SERIALIZE(s, probability);
}

void Burst::Deserialize(Serializer* s)
{
    DESERIALIZE(s, time);
    DESERIALIZE(s, count);
    DESERIALIZE(s, cycles);
    DESERIALIZE(s, interval);
    DESERIALIZE(s, probability);
}

void EmissionModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, rateOverTime);
    SERIALIZE(s, rateOverDistance);
    SERIALIZE(s, bursts);
}

void EmissionModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, rateOverTime);
    DESERIALIZE(s, rateOverDistance);
    DESERIALIZE(s, bursts);
}

void ShapeModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SerializeEnum(s, "shape", shape);
    SerializeEnum(s, "sampling", sampling);
    SerializeEnum(s, "direction", direction);
    SERIALIZE(s, axis);
    SERIALIZE(s, rotation);
    SERIALIZE(s, radius);
    SERIALIZE(s, size);
    SERIALIZE(s, coneAngle);
    SERIALIZE(s, coneLength);
    SERIALIZE(s, shell);
    SERIALIZE(s, poissonMinDistance);
}

void ShapeModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DeserializeEnum(s, "shape", shape);
    DeserializeEnum(s, "sampling", sampling);
    DeserializeEnum(s, "direction", direction);
    DESERIALIZE(s, axis);
    DESERIALIZE(s, rotation);
    DESERIALIZE(s, radius);
    DESERIALIZE(s, size);
    DESERIALIZE(s, coneAngle);
    DESERIALIZE(s, coneLength);
    DESERIALIZE(s, shell);
    DESERIALIZE(s, poissonMinDistance);
}

void VelocityOverLifetimeModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, velocity);
    SERIALIZE(s, speedMultiplier);
    SERIALIZE(s, inheritEmitterVelocity);
}

void VelocityOverLifetimeModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, velocity);
    DESERIALIZE(s, speedMultiplier);
    DESERIALIZE(s, inheritEmitterVelocity);
}

void ColorOverLifetimeModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, color);
}

void ColorOverLifetimeModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, color);
}

void SizeOverLifetimeModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, size);
}

void SizeOverLifetimeModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, size);
}

void RotationOverLifetimeModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, angularVelocity);
}

void RotationOverLifetimeModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, angularVelocity);
}

void NoiseModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, strength);
    SERIALIZE(s, frequency);
    SERIALIZE(s, scrollVelocity);
    SERIALIZE(s, octaves);
    SERIALIZE(s, octaveStrengthMultiplier);
    SERIALIZE(s, octaveFrequencyMultiplier);
    SERIALIZE(s, damping);
    SerializeEnum(s, "space", space);
    SERIALIZE(s, positionInfluence);
    SERIALIZE(s, rotationInfluence);
    SERIALIZE(s, sizeInfluence);
    SERIALIZE(s, seedOffset);
}

void NoiseModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, strength);
    DESERIALIZE(s, frequency);
    DESERIALIZE(s, scrollVelocity);
    DESERIALIZE(s, octaves);
    DESERIALIZE(s, octaveStrengthMultiplier);
    DESERIALIZE(s, octaveFrequencyMultiplier);
    DESERIALIZE(s, damping);
    DeserializeEnum(s, "space", space);
    DESERIALIZE(s, positionInfluence);
    DESERIALIZE(s, rotationInfluence);
    DESERIALIZE(s, sizeInfluence);
    DESERIALIZE(s, seedOffset);
}

void LimitVelocityModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, speedLimit);
    SERIALIZE(s, drag);
    SERIALIZE(s, dampen);
}

void LimitVelocityModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, speedLimit);
    DESERIALIZE(s, drag);
    DESERIALIZE(s, dampen);
}

TextureSheetModule::TextureSheetModule()
{
    frameOverLifetime.mode = ScalarMode::Curve;
    frameOverLifetime.curveMax.keys = {{0.0f, 0.0f}, {1.0f, 1.0f}};
}

void TextureSheetModule::Serialize(Serializer* s) const
{
    SERIALIZE(s, enabled);
    SERIALIZE(s, columns);
    SERIALIZE(s, rows);
    SERIALIZE(s, frameOverLifetime);
    SERIALIZE(s, cycles);
    SERIALIZE(s, randomStartFrame);
}

void TextureSheetModule::Deserialize(Serializer* s)
{
    DESERIALIZE(s, enabled);
    DESERIALIZE(s, columns);
    DESERIALIZE(s, rows);
    DESERIALIZE(s, frameOverLifetime);
    DESERIALIZE(s, cycles);
    DESERIALIZE(s, randomStartFrame);
}

void RendererModule::Serialize(Serializer* s) const
{
    SerializeEnum(s, "mode", mode);
    SerializeEnum(s, "billboardMode", billboardMode);
    SerializeEnum(s, "sortMode", sortMode);
    SERIALIZE(s, material);
    SERIALIZE(s, mesh);
    SERIALIZE(s, pivot);
    SERIALIZE(s, stretchScale);
    SERIALIZE(s, velocityScale);
    SERIALIZE(s, sortBias);
    SERIALIZE(s, minScreenSize);
    SERIALIZE(s, maxScreenSize);
}

void RendererModule::Deserialize(Serializer* s)
{
    DeserializeEnum(s, "mode", mode);
    DeserializeEnum(s, "billboardMode", billboardMode);
    DeserializeEnum(s, "sortMode", sortMode);
    DESERIALIZE(s, material);
    DESERIALIZE(s, mesh);
    DESERIALIZE(s, pivot);
    DESERIALIZE(s, stretchScale);
    DESERIALIZE(s, velocityScale);
    DESERIALIZE(s, sortBias);
    DESERIALIZE(s, minScreenSize);
    DESERIALIZE(s, maxScreenSize);
}

void PerformanceSettings::Serialize(Serializer* s) const
{
    SERIALIZE(s, multithreadedSimulation);
}

void PerformanceSettings::Deserialize(Serializer* s)
{
    DESERIALIZE(s, multithreadedSimulation);
}

void PreviewSettings::Serialize(Serializer* s) const
{
    SERIALIZE(s, showShape);
    SERIALIZE(s, showBounds);
    SERIALIZE(s, shapeSampleCount);
    SERIALIZE(s, sampleMarkerSize);
}

void PreviewSettings::Deserialize(Serializer* s)
{
    DESERIALIZE(s, showShape);
    DESERIALIZE(s, showBounds);
    DESERIALIZE(s, shapeSampleCount);
    DESERIALIZE(s, sampleMarkerSize);
}
} // namespace Particles

void ParticleSystem::Storage::Resize(size_t capacity)
{
    positions.resize(capacity);
    velocities.resize(capacity);
    startSizes.resize(capacity, float3(1.0f));
    sizes.resize(capacity, float3(1.0f));
    rotations.resize(capacity, glm::identity<glm::quat>());
    startColors.resize(capacity, float4(1.0f));
    colors.resize(capacity, float4(1.0f));
    ages.resize(capacity);
    lifetimes.resize(capacity, 1.0f);
    textureFrames.resize(capacity);
    randomSeeds.resize(capacity);
    spawnIds.resize(capacity);
    poissonSampleIndices.resize(capacity, -1);
    alive.resize(capacity);
    Clear();
}

void ParticleSystem::Storage::Clear()
{
    std::fill(alive.begin(), alive.end(), uint8_t(0));
    std::fill(poissonSampleIndices.begin(), poissonSampleIndices.end(), -1);
    activeSlots.clear();
    freeSlots.resize(alive.size());
    std::iota(freeSlots.rbegin(), freeSlots.rend(), uint32_t(0));
}

ParticleSystem::ParticleSystem()
    : ParticleSystem(nullptr) {}

ParticleSystem::ParticleSystem(GameObject* gameObject)
    : Component(gameObject) {}

ParticleSystem::~ParticleSystem() = default;

void ParticleSystem::OnAwake()
{
    InitializeStorage();
    InitializeGpuState();
    if (main.playOnAwake)
        Play();
}

void ParticleSystem::OnEnable()
{
    if (Scene* scene = GetScene())
        scene->GetRenderingScene().AddRenderObject(*this);
}

void ParticleSystem::OnDisable()
{
    if (Scene* scene = GetScene())
        scene->GetRenderingScene().RemoveRenderObject(*this);
}

void ParticleSystem::Tick()
{
    if (playbackState == Particles::PlaybackState::Playing)
        Simulate(Time::DeltaTime());
}

void ParticleSystem::IdleTick()
{
    if (playbackState == Particles::PlaybackState::Playing)
        Simulate(Time::DeltaTime());
}

void ParticleSystem::DebugDraw() {}

void ParticleSystem::OnDrawGizmos()
{
    if (gameObject == nullptr)
        return;

    if (preview.showShape && shape.enabled)
    {
        const float4x4 emitterToWorld = GetEmitterWorldMatrix();
        const glm::mat3 emitterBasis = NormalizeBasis(glm::mat3(emitterToWorld));
        const float markerSize = std::max(preview.sampleMarkerSize, 0.001f);
        const int sampleCount = std::clamp(preview.shapeSampleCount, 1, 256);
        for (const float3& sample : GenerateShapePreviewSamples(static_cast<size_t>(sampleCount)))
        {
            const float3 worldPosition = float3(emitterToWorld * float4(sample, 1.0f));
            Graphics::DrawLine(
                worldPosition - emitterBasis[0] * markerSize,
                worldPosition + emitterBasis[0] * markerSize,
                float4(1.0f, 0.55f, 0.1f, 1.0f)
            );
            Graphics::DrawLine(
                worldPosition - emitterBasis[1] * markerSize,
                worldPosition + emitterBasis[1] * markerSize,
                float4(1.0f, 0.55f, 0.1f, 1.0f)
            );
            Graphics::DrawLine(
                worldPosition - emitterBasis[2] * markerSize,
                worldPosition + emitterBasis[2] * markerSize,
                float4(1.0f, 0.55f, 0.1f, 1.0f)
            );
        }
    }

    if (preview.showBounds)
    {
        const AABB& bounds = GetBounds();
        const float3 corners[8] = {
            {bounds.min.x, bounds.min.y, bounds.min.z},
            {bounds.max.x, bounds.min.y, bounds.min.z},
            {bounds.max.x, bounds.max.y, bounds.min.z},
            {bounds.min.x, bounds.max.y, bounds.min.z},
            {bounds.min.x, bounds.min.y, bounds.max.z},
            {bounds.max.x, bounds.min.y, bounds.max.z},
            {bounds.max.x, bounds.max.y, bounds.max.z},
            {bounds.min.x, bounds.max.y, bounds.max.z}
        };
        constexpr int edges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };
        for (const auto& edge : edges)
            Graphics::DrawLine(corners[edge[0]], corners[edge[1]], float4(0.15f, 0.8f, 1.0f, 1.0f));
    }
}

void ParticleSystem::Serialize(Serializer* ser) const
{
    Component::Serialize(ser);
    SERIALIZE(ser, particleSystemVersion);
    SERIALIZE(ser, main);
    SERIALIZE(ser, emission);
    SERIALIZE(ser, shape);
    SERIALIZE(ser, velocityOverLifetime);
    SERIALIZE(ser, colorOverLifetime);
    SERIALIZE(ser, sizeOverLifetime);
    SERIALIZE(ser, rotationOverLifetime);
    SERIALIZE(ser, noise);
    SERIALIZE(ser, limitVelocity);
    SERIALIZE(ser, textureSheet);
    SERIALIZE(ser, renderer);
    SERIALIZE(ser, performance);
    SERIALIZE(ser, preview);
}

void ParticleSystem::Deserialize(Serializer* ser)
{
    Component::Deserialize(ser);
    DESERIALIZE(ser, particleSystemVersion);
    DESERIALIZE(ser, main);
    DESERIALIZE(ser, emission);
    DESERIALIZE(ser, shape);
    DESERIALIZE(ser, velocityOverLifetime);
    DESERIALIZE(ser, colorOverLifetime);
    DESERIALIZE(ser, sizeOverLifetime);
    DESERIALIZE(ser, rotationOverLifetime);
    DESERIALIZE(ser, noise);
    DESERIALIZE(ser, limitVelocity);
    DESERIALIZE(ser, textureSheet);
    DESERIALIZE(ser, renderer);
    DESERIALIZE(ser, performance);
    DESERIALIZE(ser, preview);
    particleSystemVersion = CurrentVersion;
    NotifySettingsChanged();
}

void ParticleSystem::InitializeStorage()
{
    main.maxParticles = std::max(main.maxParticles, 1);
    storage.Resize(static_cast<size_t>(main.maxParticles));
    deathFlags.reserve(static_cast<size_t>(main.maxParticles));
    renderParticles.reserve(static_cast<size_t>(main.maxParticles));
    sortedRenderParticles.reserve(static_cast<size_t>(main.maxParticles));
    stats.activeParticles = 0;
}

void ParticleSystem::InitializeGpuState()
{
    if (gpuStateInitialized)
        return;

    const size_t bufferSize = sizeof(GPUParticle) * static_cast<size_t>(main.maxParticles);
    particleBuffer = GetGfxDriver()->CreateBuffer(bufferSize, Gfx::BufferUsage::Storage, false, false, "Particles");
    particleMaterial = std::make_unique<Material>();
    gpuStateInitialized = true;
    materialDirty = true;
    RefreshMaterial();
}

void ParticleSystem::RefreshMaterial()
{
    if (!gpuStateInitialized || !materialDirty)
        return;

    Shader* particleShader = ShaderLibrary::GetShader(Shaders::Particle);
    if (renderer.material != nullptr && renderer.material->GetShader().Get() == particleShader)
        particleMaterial->Copy(*renderer.material);
    else
        particleMaterial->SetShader(particleShader);

    particleMaterial->SetBuffer("particles", particleBuffer.get());
    if (particleMaterial->GetTexture("mainTexture") == nullptr)
        particleMaterial->SetTexture("mainTexture", &EngineInternalResources::GetWhiteTexture());
    particleMaterial->SetTextureSamplerIndex("sampler_linear_clamp", 5);
    materialDirty = false;
}

void ParticleSystem::NotifySettingsChanged()
{
    main.duration = std::max(main.duration, MinimumLifetime);
    main.simulationSpeed = std::max(main.simulationSpeed, 0.0f);
    main.fixedDeltaTime = std::max(main.fixedDeltaTime, MinimumLifetime);
    main.maxParticles = std::max(main.maxParticles, 1);
    shape.radius = std::max(shape.radius, 0.0f);
    shape.size = glm::max(shape.size, float3(0.0f));
    shape.coneLength = std::max(shape.coneLength, 0.0f);
    shape.coneAngle = std::clamp(shape.coneAngle, 0.0f, 89.9f);
    shape.poissonMinDistance = std::max(shape.poissonMinDistance, 0.0f);
    for (Particles::Burst& burst : emission.bursts)
    {
        burst.time = std::max(burst.time, 0.0f);
        burst.count = std::max(burst.count, 0);
        burst.cycles = std::max(burst.cycles, 0);
        burst.interval = std::max(burst.interval, 0.0f);
        burst.probability = std::clamp(burst.probability, 0.0f, 1.0f);
    }
    noise.frequency = std::max(noise.frequency, 0.0f);
    noise.octaves = std::clamp(noise.octaves, 1, 8);
    noise.octaveFrequencyMultiplier = std::max(noise.octaveFrequencyMultiplier, 0.0f);
    noise.damping = std::max(noise.damping, 0.0f);
    limitVelocity.dampen = std::clamp(limitVelocity.dampen, 0.0f, 1.0f);
    textureSheet.columns = std::max(textureSheet.columns, 1);
    textureSheet.rows = std::max(textureSheet.rows, 1);
    textureSheet.cycles = std::max(textureSheet.cycles, 0.0f);
    renderer.stretchScale = std::max(renderer.stretchScale, 0.0f);
    renderer.velocityScale = std::max(renderer.velocityScale, 0.0f);
    renderer.minScreenSize = std::max(renderer.minScreenSize, 0.0f);
    renderer.maxScreenSize = std::max(renderer.maxScreenSize, renderer.minScreenSize);
    preview.shapeSampleCount = std::clamp(preview.shapeSampleCount, 1, 256);
    preview.sampleMarkerSize = std::max(preview.sampleMarkerSize, 0.001f);

    if (storage.positions.size() != static_cast<size_t>(main.maxParticles))
    {
        Stop();
        InitializeStorage();
        if (gpuStateInitialized)
        {
            particleBuffer = GetGfxDriver()->CreateBuffer(
                sizeof(GPUParticle) * static_cast<size_t>(main.maxParticles),
                Gfx::BufferUsage::Storage,
                false,
                false,
                "Particles"
            );
        }
    }

    poissonSignature = 0;
    materialDirty = true;
}

void ParticleSystem::SetParticleCount(int count)
{
    main.maxParticles = std::max(count, 1);
    NotifySettingsChanged();
}

void ParticleSystem::SetParticleMesh(Mesh* mesh)
{
    renderer.mesh = mesh;
    renderer.mode = Particles::RendererMode::Mesh;
}

void ParticleSystem::Play()
{
    if (playbackState == Particles::PlaybackState::Paused)
    {
        playbackState = Particles::PlaybackState::Playing;
        return;
    }

    Stop();
    if (storage.positions.size() != static_cast<size_t>(main.maxParticles))
        InitializeStorage();
    runtimeSeed = main.automaticSeed ? Hash(static_cast<uint32_t>(Random::GenerateNormalized() * 4294967295.0f))
                                     : main.seed;
    startDelayRemaining = std::max(main.startDelay.Evaluate(0.0f, Random01(runtimeSeed, 0, 0)), 0.0f);
    previousEmitterPosition = GetEmitterPosition();
    automaticBounds = AABB(previousEmitterPosition, previousEmitterPosition);
    ResetBurstState();
    playbackState = Particles::PlaybackState::Playing;

    if (main.prewarm && main.looping && main.duration > 0.0f)
    {
        const float step = main.fixedDeltaTime;
        float remaining = main.duration;
        while (remaining > 0.0f)
        {
            const float currentStep = std::min(step, remaining);
            SimulateStep(currentStep);
            remaining -= currentStep;
        }
    }
}

void ParticleSystem::Pause()
{
    if (playbackState == Particles::PlaybackState::Playing)
        playbackState = Particles::PlaybackState::Paused;
}

void ParticleSystem::Stop()
{
    playbackState = Particles::PlaybackState::Stopped;
    systemTime = 0.0f;
    previousSystemTime = 0.0f;
    fixedTimeAccumulator = 0.0f;
    timeEmissionAccumulator = 0.0f;
    distanceEmissionAccumulator = 0.0f;
    nextSpawnId = 1;
    loopIndex = 0;
    storage.Clear();
    freePoissonSampleIndices.resize(poissonSamples.size());
    std::iota(freePoissonSampleIndices.rbegin(), freePoissonSampleIndices.rend(), uint32_t(0));
    renderParticles.clear();
    sortedRenderParticles.clear();
    stats.activeParticles = 0;
    stats.capacitySaturated = false;
}

void ParticleSystem::Restart()
{
    Stop();
    Play();
}

void ParticleSystem::Simulate(float deltaTime)
{
    if (main.cullingMode == Particles::CullingMode::PauseWhenCulled && !stats.visible)
        return;

    deltaTime = std::max(deltaTime * main.simulationSpeed, 0.0f);
    if (main.fixedTimeStep)
    {
        fixedTimeAccumulator += deltaTime;
        while (fixedTimeAccumulator >= main.fixedDeltaTime)
        {
            SimulateStep(main.fixedDeltaTime);
            fixedTimeAccumulator -= main.fixedDeltaTime;
        }
    }
    else if (deltaTime > 0.0f)
    {
        SimulateStep(deltaTime);
    }
}

void ParticleSystem::ResetBurstState()
{
    nextBurstTimes.resize(emission.bursts.size());
    remainingBurstCycles.resize(emission.bursts.size());
    for (size_t i = 0; i < emission.bursts.size(); ++i)
    {
        nextBurstTimes[i] = std::max(emission.bursts[i].time, 0.0f);
        remainingBurstCycles[i] = std::max(emission.bursts[i].cycles, 0);
    }
}

int ParticleSystem::CalculateEmission(float deltaTime)
{
    if (!emission.enabled || (main.duration > 0.0f && !main.looping && systemTime >= main.duration))
        return 0;

    const float normalizedSystemTime = main.duration > 0.0f ? glm::clamp(systemTime / main.duration, 0.0f, 1.0f) : 0.0f;
    timeEmissionAccumulator += std::max(
        emission.rateOverTime.Evaluate(normalizedSystemTime, Random01(runtimeSeed, loopIndex, 100)),
        0.0f
    ) * deltaTime;

    const float distance = glm::distance(previousEmitterPosition, GetEmitterPosition());
    distanceEmissionAccumulator += std::max(
        emission.rateOverDistance.Evaluate(normalizedSystemTime, Random01(runtimeSeed, loopIndex, 101)),
        0.0f
    ) * distance;

    int count = static_cast<int>(timeEmissionAccumulator) + static_cast<int>(distanceEmissionAccumulator);
    timeEmissionAccumulator -= std::floor(timeEmissionAccumulator);
    distanceEmissionAccumulator -= std::floor(distanceEmissionAccumulator);

    const float endTime = systemTime + deltaTime;
    for (size_t burstIndex = 0; burstIndex < emission.bursts.size(); ++burstIndex)
    {
        const Particles::Burst& burst = emission.bursts[burstIndex];
        while (remainingBurstCycles[burstIndex] > 0 && nextBurstTimes[burstIndex] <= endTime)
        {
            if (nextBurstTimes[burstIndex] >= systemTime)
            {
                const int cycle = burst.cycles - remainingBurstCycles[burstIndex];
                if (Random01(runtimeSeed, loopIndex, static_cast<uint32_t>(200 + burstIndex * 31 + cycle)) <=
                    glm::clamp(burst.probability, 0.0f, 1.0f))
                    count += std::max(burst.count, 0);
            }
            --remainingBurstCycles[burstIndex];
            nextBurstTimes[burstIndex] += std::max(burst.interval, 0.0f);
        }
    }
    return count;
}

void ParticleSystem::SimulateStep(float deltaTime)
{
    const auto simulationStart = std::chrono::steady_clock::now();
    stats.emittedThisFrame = 0;
    stats.droppedParticles = 0;
    stats.scheduledJobs = 0;

    const float3 emitterPosition = GetEmitterPosition();
    emitterVelocity = deltaTime > 0.0f ? (emitterPosition - previousEmitterPosition) / deltaTime : float3(0.0f);

    if (startDelayRemaining > 0.0f)
    {
        startDelayRemaining -= deltaTime;
    }
    else
    {
        const int emissionCount = CalculateEmission(deltaTime);
        SpawnParticles(emissionCount);
    }

    previousSystemTime = systemTime;
    systemTime += deltaTime;
    if (main.duration > 0.0f && systemTime >= main.duration)
    {
        if (main.looping)
        {
            while (systemTime >= main.duration)
            {
                systemTime -= main.duration;
                ++loopIndex;
            }
            previousSystemTime = 0.0f;
            ResetBurstState();
        }
        else
        {
            systemTime = main.duration;
        }
    }

    const size_t activeCount = storage.activeSlots.size();
    deathFlags.assign(activeCount, uint8_t(0));
    renderParticles.resize(activeCount);

    const float4x4 emitterToWorld = GetEmitterWorldMatrix();
    const float4x4 simulationToWorld = main.simulationSpace == Particles::SimulationSpace::Local
                                              ? emitterToWorld
                                              : float4x4(1.0f);

    const bool runParallel = performance.multithreadedSimulation && activeCount >= ParallelParticleThreshold;
    if (runParallel)
    {
        const size_t workerCount = static_cast<size_t>(std::max(JobSystem::GetTotalWorkers(), 1));
        const size_t jobCount = std::min(workerCount, (activeCount + ParallelParticleThreshold - 1) /
                                                         ParallelParticleThreshold);
        const size_t chunkSize = (activeCount + jobCount - 1) / jobCount;
        chunkResults.assign(jobCount, ChunkResult{});
        std::vector<JobHandle> handles;
        handles.reserve(jobCount);
        for (size_t jobIndex = 0; jobIndex < jobCount; ++jobIndex)
        {
            const size_t begin = jobIndex * chunkSize;
            const size_t end = std::min(begin + chunkSize, activeCount);
            if (begin >= end)
                break;
            handles.push_back(JobSystem::Instance().Schedule(
                [this, begin, end, deltaTime, simulationToWorld, emitterToWorld, jobIndex]()
                { SimulateRange(begin, end, deltaTime, simulationToWorld, emitterToWorld, chunkResults[jobIndex]); }
            ));
        }
        stats.scheduledJobs = static_cast<int>(handles.size());
        for (JobHandle& handle : handles)
            handle.Wait();
    }
    else
    {
        chunkResults.assign(1, ChunkResult{});
        SimulateRange(0, activeCount, deltaTime, simulationToWorld, emitterToWorld, chunkResults[0]);
    }

    ReclaimDeadParticles();
    UpdateBounds();
    previousEmitterPosition = emitterPosition;
    stats.activeParticles = static_cast<int>(storage.activeSlots.size());
    stats.capacitySaturated = stats.droppedParticles > 0;
    if (!main.looping && systemTime >= main.duration && storage.activeSlots.empty())
        playbackState = Particles::PlaybackState::Stopped;
    stats.simulationMilliseconds = std::chrono::duration<float, std::milli>(
                                               std::chrono::steady_clock::now() - simulationStart
    ).count();
}

void ParticleSystem::SpawnParticles(int count)
{
    int available = static_cast<int>(storage.freeSlots.size());
    if (shape.enabled && shape.sampling == Particles::ShapeSampling::PoissonDisk &&
        shape.poissonMinDistance > 0.0f)
    {
        EnsurePoissonSamples();
        available = std::min(available, static_cast<int>(freePoissonSampleIndices.size()));
    }
    const int spawnCount = std::min(count, available);
    stats.emittedThisFrame = spawnCount;
    stats.droppedParticles = std::max(count - spawnCount, 0);
    for (int i = 0; i < spawnCount; ++i)
    {
        const uint32_t slot = storage.freeSlots.back();
        storage.freeSlots.pop_back();
        SpawnParticle(slot);
        storage.activeSlots.push_back(slot);
    }
}

void ParticleSystem::SpawnParticle(uint32_t slot)
{
    const uint64_t spawnId = nextSpawnId++;
    const float3 randomA = Random3(runtimeSeed, spawnId, 0);
    const float3 randomB = Random3(runtimeSeed, spawnId, 4);
    const float3 localPosition = shape.enabled ? SampleShape(slot, spawnId) : float3(0.0f);
    float3 localDirection = shape.enabled ? SampleDirection(localPosition, spawnId) : float3(0.0f, 1.0f, 0.0f);
    if (shape.enabled && shape.direction == Particles::ShapeDirection::Axis)
        localDirection = glm::quat(glm::radians(shape.rotation)) * localDirection;

    const float4x4 worldMatrix = GetEmitterWorldMatrix();
    storage.positions[slot] = main.simulationSpace == Particles::SimulationSpace::World
                                      ? float3(worldMatrix * float4(localPosition, 1.0f))
                                      : localPosition;

    const float speed = main.startSpeed.Evaluate(0.0f, randomA.x);
    float3 initialVelocity = localDirection * speed;
    if (main.simulationSpace == Particles::SimulationSpace::World)
        initialVelocity = glm::mat3(worldMatrix) * initialVelocity;
    else if (velocityOverLifetime.inheritEmitterVelocity != 0.0f)
        initialVelocity += glm::inverse(glm::mat3(worldMatrix)) * emitterVelocity *
                           velocityOverLifetime.inheritEmitterVelocity;

    if (main.simulationSpace == Particles::SimulationSpace::World)
        initialVelocity += emitterVelocity * velocityOverLifetime.inheritEmitterVelocity;

    storage.velocities[slot] = initialVelocity;
    storage.startSizes[slot] = main.startSize.Evaluate(0.0f, randomA);
    storage.sizes[slot] = storage.startSizes[slot];
    const float3 rotationDegrees = main.startRotation.Evaluate(0.0f, randomB);
    storage.rotations[slot] = glm::quat(glm::radians(rotationDegrees));
    storage.startColors[slot] = main.startColor.Evaluate(0.0f, randomA.z);
    storage.colors[slot] = storage.startColors[slot];
    storage.ages[slot] = 0.0f;
    storage.lifetimes[slot] = std::max(main.startLifetime.Evaluate(0.0f, randomB.x), MinimumLifetime);
    storage.textureFrames[slot] = 0.0f;
    storage.randomSeeds[slot] = Hash(runtimeSeed ^ static_cast<uint32_t>(spawnId));
    storage.spawnIds[slot] = spawnId;
    storage.alive[slot] = 1;
}

void ParticleSystem::SimulateRange(
    size_t begin,
    size_t end,
    float deltaTime,
    const float4x4& simulationToWorld,
    const float4x4& emitterToWorld,
    ChunkResult& result
)
{
    const glm::mat3 simulationRotationScale(simulationToWorld);
    const glm::mat3 worldToSimulation = glm::inverse(simulationRotationScale);
    const glm::mat3 simulationRotationOnly = NormalizeBasis(simulationRotationScale);
    const glm::quat simulationRotation = glm::quat_cast(simulationRotationOnly);
    const glm::mat3 emitterRotationScale(emitterToWorld);
    const glm::mat3 emitterRotationOnly = NormalizeBasis(emitterRotationScale);
    const glm::mat3 worldToEmitterRotation = glm::transpose(emitterRotationOnly);
    const float4x4 worldToEmitter = glm::inverse(emitterToWorld);

    for (size_t denseIndex = begin; denseIndex < end; ++denseIndex)
    {
        const uint32_t slot = storage.activeSlots[denseIndex];
        storage.ages[slot] += deltaTime;
        if (storage.ages[slot] >= storage.lifetimes[slot])
        {
            deathFlags[denseIndex] = 1;
            continue;
        }

        const float normalizedAge = glm::clamp(storage.ages[slot] / storage.lifetimes[slot], 0.0f, 1.0f);
        const uint64_t spawnId = storage.spawnIds[slot];
        const float3 randomA = Random3(storage.randomSeeds[slot], spawnId, 300);
        const float3 randomB = Random3(storage.randomSeeds[slot], spawnId, 304);

        const float gravity = main.gravityModifier.Evaluate(normalizedAge, randomA.x);
        float3 gravityVector(0.0f, -9.81f * gravity, 0.0f);
        if (main.simulationSpace == Particles::SimulationSpace::Local)
            gravityVector = worldToSimulation * gravityVector;
        storage.velocities[slot] += gravityVector * deltaTime;

        float3 noiseValue(0.0f);
        if (noise.enabled)
        {
            float3 noisePosition = storage.positions[slot];
            if (noise.space == Particles::SimulationSpace::World &&
                main.simulationSpace == Particles::SimulationSpace::Local)
                noisePosition = float3(simulationToWorld * float4(noisePosition, 1.0f));
            else if (noise.space == Particles::SimulationSpace::Local &&
                     main.simulationSpace == Particles::SimulationSpace::World)
                noisePosition = float3(worldToEmitter * float4(noisePosition, 1.0f));

            noiseValue = CurlNoise(noisePosition, storage.randomSeeds[slot] + noise.seedOffset, noise, systemTime);
            if (noise.space == Particles::SimulationSpace::World &&
                main.simulationSpace == Particles::SimulationSpace::Local)
                noiseValue = worldToEmitterRotation * noiseValue;
            else if (noise.space == Particles::SimulationSpace::Local &&
                     main.simulationSpace == Particles::SimulationSpace::World)
                noiseValue = emitterRotationOnly * noiseValue;

            const float3 strength = noise.strength.Evaluate(normalizedAge, randomA);
            storage.velocities[slot] += noiseValue * strength * noise.positionInfluence * deltaTime;
            storage.velocities[slot] *= std::max(1.0f - noise.damping * deltaTime, 0.0f);
            if (noise.rotationInfluence != 0.0f)
            {
                const float3 noiseRotation = noiseValue * strength * noise.rotationInfluence * deltaTime;
                storage.rotations[slot] = glm::normalize(glm::quat(noiseRotation) * storage.rotations[slot]);
            }
        }

        if (limitVelocity.enabled)
        {
            const float drag = std::max(limitVelocity.drag.Evaluate(normalizedAge, randomB.x), 0.0f);
            storage.velocities[slot] *= std::max(1.0f - drag * deltaTime, 0.0f);
            const float speedLimit = std::max(limitVelocity.speedLimit.Evaluate(normalizedAge, randomB.y), 0.0f);
            const float currentSpeed = glm::length(storage.velocities[slot]);
            if (currentSpeed > speedLimit && currentSpeed > 0.0f)
            {
                const float targetSpeed = glm::mix(currentSpeed, speedLimit, glm::clamp(limitVelocity.dampen, 0.0f, 1.0f));
                storage.velocities[slot] *= targetSpeed / currentSpeed;
            }
        }

        float3 overLifetimeVelocity(0.0f);
        float speedMultiplier = 1.0f;
        if (velocityOverLifetime.enabled)
        {
            overLifetimeVelocity = velocityOverLifetime.velocity.Evaluate(normalizedAge, randomA);
            speedMultiplier = velocityOverLifetime.speedMultiplier.Evaluate(normalizedAge, randomB.z);
        }
        storage.positions[slot] += (storage.velocities[slot] + overLifetimeVelocity) * speedMultiplier * deltaTime;

        if (rotationOverLifetime.enabled)
        {
            const float3 angularVelocity = glm::radians(
                rotationOverLifetime.angularVelocity.Evaluate(normalizedAge, randomB)
            );
            storage.rotations[slot] = glm::normalize(glm::quat(angularVelocity * deltaTime) * storage.rotations[slot]);
        }

        float3 sizeMultiplier(1.0f);
        if (sizeOverLifetime.enabled)
            sizeMultiplier = sizeOverLifetime.size.Evaluate(normalizedAge, randomA);
        if (noise.enabled && noise.sizeInfluence != 0.0f)
            sizeMultiplier *= glm::max(float3(0.0f), float3(1.0f) + noiseValue * noise.sizeInfluence);
        storage.sizes[slot] = storage.startSizes[slot] * sizeMultiplier;

        storage.colors[slot] = storage.startColors[slot];
        if (colorOverLifetime.enabled)
            storage.colors[slot] *= colorOverLifetime.color.Evaluate(normalizedAge, randomA.z);

        if (textureSheet.enabled)
        {
            const float randomStart = textureSheet.randomStartFrame ? randomB.x : 0.0f;
            const float frameProgress = textureSheet.frameOverLifetime.Evaluate(normalizedAge, randomA.y) *
                                            textureSheet.cycles +
                                        randomStart;
            storage.textureFrames[slot] = glm::fract(frameProgress) *
                                          static_cast<float>(textureSheet.columns * textureSheet.rows);
        }

        const float3 worldPosition = main.simulationSpace == Particles::SimulationSpace::Local
                                             ? float3(simulationToWorld * float4(storage.positions[slot], 1.0f))
                                             : storage.positions[slot];
        float3 worldVelocity = main.simulationSpace == Particles::SimulationSpace::Local
                                       ? simulationRotationScale * storage.velocities[slot]
                                       : storage.velocities[slot];
        glm::quat worldRotation = main.simulationSpace == Particles::SimulationSpace::Local
                                          ? simulationRotation * storage.rotations[slot]
                                          : storage.rotations[slot];
        float3 worldSize = storage.sizes[slot];
        if (main.simulationSpace == Particles::SimulationSpace::Local)
        {
            worldSize *= float3(
                glm::length(simulationRotationScale[0]),
                glm::length(simulationRotationScale[1]),
                glm::length(simulationRotationScale[2])
            );
        }

        float renderMode = 0.0f;
        if (renderer.mode == Particles::RendererMode::Billboard)
            renderMode = static_cast<float>(renderer.billboardMode) + 1.0f;

        GPUParticle& gpuParticle = renderParticles[denseIndex];
        gpuParticle.positionAndFrame = float4(worldPosition, storage.textureFrames[slot]);
        gpuParticle.rotation = float4(worldRotation.x, worldRotation.y, worldRotation.z, worldRotation.w);
        gpuParticle.scaleAndMode = float4(worldSize, renderMode);
        gpuParticle.color = storage.colors[slot];
        gpuParticle.velocityAndStretch = float4(
            worldVelocity,
            renderer.stretchScale + renderer.velocityScale * glm::length(worldVelocity)
        );
        gpuParticle.sortData = float4(
            normalizedAge,
            static_cast<float>(spawnId & 0x00FFFFFFu),
            glm::eulerAngles(storage.rotations[slot]).z,
            0.0f
        );

        float extent = glm::compMax(glm::abs(worldSize)) * 0.5f;
        if (renderer.mode == Particles::RendererMode::Billboard &&
            renderer.billboardMode == Particles::BillboardMode::Stretched)
            extent = std::max(extent, std::abs(worldSize.y * gpuParticle.velocityAndStretch.w) * 0.5f);
        result.min = glm::min(result.min, worldPosition - extent);
        result.max = glm::max(result.max, worldPosition + extent);
        result.hasBounds = true;
    }
}

void ParticleSystem::ReclaimDeadParticles()
{
    size_t writeIndex = 0;
    for (size_t denseIndex = 0; denseIndex < storage.activeSlots.size(); ++denseIndex)
    {
        const uint32_t slot = storage.activeSlots[denseIndex];
        if (deathFlags[denseIndex])
        {
            storage.alive[slot] = 0;
            if (storage.poissonSampleIndices[slot] >= 0)
            {
                freePoissonSampleIndices.push_back(static_cast<uint32_t>(storage.poissonSampleIndices[slot]));
                storage.poissonSampleIndices[slot] = -1;
            }
            storage.freeSlots.push_back(slot);
            continue;
        }

        storage.activeSlots[writeIndex] = slot;
        renderParticles[writeIndex] = renderParticles[denseIndex];
        ++writeIndex;
    }
    storage.activeSlots.resize(writeIndex);
    renderParticles.resize(writeIndex);
}

void ParticleSystem::UpdateBounds()
{
    bool hasBounds = false;
    float3 boundsMin(std::numeric_limits<float>::max());
    float3 boundsMax(std::numeric_limits<float>::lowest());
    for (const ChunkResult& result : chunkResults)
    {
        if (!result.hasBounds)
            continue;
        boundsMin = glm::min(boundsMin, result.min);
        boundsMax = glm::max(boundsMax, result.max);
        hasBounds = true;
    }

    if (hasBounds)
        automaticBounds = AABB(boundsMin, boundsMax);
    else
    {
        const float3 position = GetEmitterPosition();
        automaticBounds = AABB(position, position);
    }
}

const AABB& ParticleSystem::GetBounds() const
{
    return main.automaticBounds ? automaticBounds : main.manualBounds;
}

float3 ParticleSystem::GetEmitterPosition() const
{
    return gameObject != nullptr ? gameObject->GetPosition() : float3(0.0f);
}

float4x4 ParticleSystem::GetEmitterWorldMatrix() const
{
    return gameObject != nullptr ? gameObject->GetWorldMatrix() : float4x4(1.0f);
}

bool ParticleSystem::IsVisible(const Frustum& frustum) const
{
    return AABBVsFrustum(GetBounds(), frustum);
}

void ParticleSystem::PrepareRenderData(Camera& camera)
{
    InitializeGpuState();
    RefreshMaterial();

    const auto sortStart = std::chrono::steady_clock::now();
    sortedRenderParticles = renderParticles;
    if (renderer.sortMode != Particles::SortMode::None)
    {
        const float3 cameraPosition = camera.GetGameObject()->GetPosition();
        std::stable_sort(
            sortedRenderParticles.begin(),
            sortedRenderParticles.end(),
            [&](const GPUParticle& a, const GPUParticle& b)
            {
                switch (renderer.sortMode)
                {
                    case Particles::SortMode::Distance:
                    {
                        const float aDistance = glm::distance2(float3(a.positionAndFrame), cameraPosition);
                        const float bDistance = glm::distance2(float3(b.positionAndFrame), cameraPosition);
                        return aDistance > bDistance;
                    }
                    case Particles::SortMode::YoungestFirst:
                        return a.sortData.x < b.sortData.x;
                    case Particles::SortMode::OldestFirst:
                        return a.sortData.x > b.sortData.x;
                    case Particles::SortMode::None: return false;
                }
                return false;
            }
        );
    }
    stats.sortingMilliseconds = std::chrono::duration<float, std::milli>(
                                            std::chrono::steady_clock::now() - sortStart
    ).count();

    const auto uploadStart = std::chrono::steady_clock::now();
    if (!sortedRenderParticles.empty())
    {
        GetGfxDriver()->UploadBuffer(
            *particleBuffer,
            reinterpret_cast<uint8_t*>(sortedRenderParticles.data()),
            sizeof(GPUParticle) * sortedRenderParticles.size(),
            0
        );
    }
    stats.uploadMilliseconds = std::chrono::duration<float, std::milli>(
                                           std::chrono::steady_clock::now() - uploadStart
    ).count();

    particleMaterial->SetVector(
        "config",
        float4(
            static_cast<float>(textureSheet.columns),
            static_cast<float>(textureSheet.rows),
            renderer.pivot.x,
            renderer.pivot.y
        )
    );
    particleMaterial->SetVector(
        "screenSizeRange",
        float4(renderer.minScreenSize, renderer.maxScreenSize, 0.0f, 0.0f)
    );
}

Rendering::ParticleDraw ParticleSystem::GetDraw(Camera& camera)
{
    PrepareRenderData(camera);
    draw.particleShaderParameters = particleMaterial->GetShaderResource();
    Mesh* drawMesh = renderer.mode == Particles::RendererMode::Mesh ? renderer.mesh.Get()
                                                                    : EngineInternalResources::GetModels().plane;
    if (drawMesh == nullptr)
        drawMesh = EngineInternalResources::GetModels().sphere;
    draw.instancingMesh = drawMesh->GetSubmesh(0);
    draw.particleCount = sortedRenderParticles.size();
    return draw;
}

std::vector<float3> ParticleSystem::GenerateShapePreviewSamples(size_t count)
{
    std::vector<float3> samples;
    samples.reserve(count);
    if (count == 0 || !shape.enabled)
        return samples;

    const uint32_t savedRuntimeSeed = runtimeSeed;
    runtimeSeed = main.seed;
    const glm::quat rotation = glm::quat(glm::radians(shape.rotation));

    if (shape.sampling == Particles::ShapeSampling::PoissonDisk && shape.poissonMinDistance > 0.0f)
    {
        const float minimumDistanceSquared = shape.poissonMinDistance * shape.poissonMinDistance;
        const size_t maxAttempts = std::max(count * 128, size_t(128));
        for (size_t attempt = 0; attempt < maxAttempts && samples.size() < count; ++attempt)
        {
            const float3 candidate = rotation * SampleRandomShape(static_cast<uint64_t>(attempt + 1));
            const bool overlaps = std::any_of(
                samples.begin(),
                samples.end(),
                [&candidate, minimumDistanceSquared](const float3& sample)
                { return glm::distance2(candidate, sample) < minimumDistanceSquared; }
            );
            if (!overlaps)
                samples.push_back(candidate);
        }
    }
    else
    {
        for (size_t i = 0; i < count; ++i)
            samples.push_back(rotation * SampleRandomShape(static_cast<uint64_t>(i + 1)));
    }

    runtimeSeed = savedRuntimeSeed;
    return samples;
}

size_t ParticleSystem::CalculateShapeSignature() const
{
    size_t signature = static_cast<size_t>(shape.shape);
    auto combine = [&signature](size_t value)
    { signature ^= value + 0x9E3779B97F4A7C15ull + (signature << 6u) + (signature >> 2u); };
    combine(static_cast<size_t>(shape.sampling));
    combine(std::hash<float>{}(shape.radius));
    combine(std::hash<float>{}(shape.size.x));
    combine(std::hash<float>{}(shape.size.y));
    combine(std::hash<float>{}(shape.size.z));
    combine(std::hash<float>{}(shape.coneAngle));
    combine(std::hash<float>{}(shape.coneLength));
    combine(std::hash<float>{}(shape.poissonMinDistance));
    combine(static_cast<size_t>(shape.shell));
    combine(static_cast<size_t>(main.maxParticles));
    return signature;
}

void ParticleSystem::EnsurePoissonSamples()
{
    const size_t signature = CalculateShapeSignature();
    if (poissonSignature == signature && !poissonSamples.empty())
        return;

    poissonSignature = signature;
    poissonSamples.clear();
    poissonSamples.reserve(static_cast<size_t>(main.maxParticles));

    const float minimumDistanceSquared = shape.poissonMinDistance * shape.poissonMinDistance;
    const size_t maxAttempts = static_cast<size_t>(main.maxParticles) * 64;
    for (size_t attempt = 0; attempt < maxAttempts && poissonSamples.size() < static_cast<size_t>(main.maxParticles);
         ++attempt)
    {
        const float3 candidate = SampleRandomShape(static_cast<uint64_t>(attempt + 1));
        bool accepted = true;
        for (const float3& sample : poissonSamples)
        {
            if (glm::distance2(candidate, sample) < minimumDistanceSquared)
            {
                accepted = false;
                break;
            }
        }
        if (accepted)
            poissonSamples.push_back(candidate);
    }

    if (poissonSamples.empty())
        poissonSamples.push_back(float3(0.0f));

    freePoissonSampleIndices.resize(poissonSamples.size());
    std::iota(freePoissonSampleIndices.rbegin(), freePoissonSampleIndices.rend(), uint32_t(0));
    for (const uint32_t slot : storage.activeSlots)
        storage.poissonSampleIndices[slot] = -1;
}

float3 ParticleSystem::SampleShape(uint32_t slot, uint64_t spawnId)
{
    if (shape.sampling == Particles::ShapeSampling::PoissonDisk && shape.poissonMinDistance > 0.0f)
    {
        EnsurePoissonSamples();
        const uint32_t sampleIndex = freePoissonSampleIndices.back();
        freePoissonSampleIndices.pop_back();
        storage.poissonSampleIndices[slot] = static_cast<int32_t>(sampleIndex);
        return glm::quat(glm::radians(shape.rotation)) * poissonSamples[sampleIndex];
    }
    storage.poissonSampleIndices[slot] = -1;
    return glm::quat(glm::radians(shape.rotation)) * SampleRandomShape(spawnId);
}

float3 ParticleSystem::SampleRandomShape(uint64_t spawnId) const
{
    const float3 random = Random3(runtimeSeed, spawnId, 20);
    const float angle = TwoPi * random.x;
    const float2 circleDirection(std::cos(angle), std::sin(angle));
    switch (shape.shape)
    {
        case Particles::ShapeType::Point: return float3(0.0f);
        case Particles::ShapeType::Disk:
        {
            const float radius = shape.shell ? shape.radius : shape.radius * std::sqrt(random.y);
            return float3(circleDirection.x * radius, 0.0f, circleDirection.y * radius);
        }
        case Particles::ShapeType::Rectangle:
            return float3((random.x - 0.5f) * shape.size.x, 0.0f, (random.y - 0.5f) * shape.size.z);
        case Particles::ShapeType::Box:
        {
            float3 point = (random - 0.5f) * shape.size;
            if (shape.shell)
            {
                const int face = static_cast<int>(Random01(runtimeSeed, spawnId, 24) * 6.0f) % 6;
                const int axis = face / 2;
                point[axis] = (face % 2 == 0 ? -0.5f : 0.5f) * shape.size[axis];
            }
            return point;
        }
        case Particles::ShapeType::Sphere:
        case Particles::ShapeType::Hemisphere:
        {
            const float z = shape.shape == Particles::ShapeType::Hemisphere ? random.y : random.y * 2.0f - 1.0f;
            const float planar = std::sqrt(std::max(1.0f - z * z, 0.0f));
            float3 direction(circleDirection.x * planar, z, circleDirection.y * planar);
            const float radius = shape.shell ? shape.radius : shape.radius * std::cbrt(random.z);
            return direction * radius;
        }
        case Particles::ShapeType::Cone:
        {
            const float height = shape.shell ? shape.coneLength : shape.coneLength * random.y;
            const float maxRadius = std::tan(glm::radians(shape.coneAngle)) * height;
            const float radius = shape.shell ? maxRadius : maxRadius * std::sqrt(random.z);
            return float3(circleDirection.x * radius, height, circleDirection.y * radius);
        }
    }
    return float3(0.0f);
}

float3 ParticleSystem::SampleDirection(const float3& position, uint64_t spawnId) const
{
    switch (shape.direction)
    {
        case Particles::ShapeDirection::Axis:
            return glm::length2(shape.axis) > 0.0f ? glm::normalize(shape.axis) : float3(0.0f, 1.0f, 0.0f);
        case Particles::ShapeDirection::Outward:
            return glm::length2(position) > 0.0f ? glm::normalize(position) : float3(0.0f, 1.0f, 0.0f);
        case Particles::ShapeDirection::Random:
        {
            const float3 random = Random3(runtimeSeed, spawnId, 28);
            const float z = random.y * 2.0f - 1.0f;
            const float angle = TwoPi * random.x;
            const float planar = std::sqrt(std::max(1.0f - z * z, 0.0f));
            return float3(std::cos(angle) * planar, z, std::sin(angle) * planar);
        }
    }
    return float3(0.0f, 1.0f, 0.0f);
}

uint32_t ParticleSystem::Hash(uint32_t value)
{
    value ^= value >> 16u;
    value *= 0x7FEB352Du;
    value ^= value >> 15u;
    value *= 0x846CA68Bu;
    value ^= value >> 16u;
    return value;
}

float ParticleSystem::Random01(uint32_t seed, uint64_t id, uint32_t stream)
{
    const uint32_t value = Hash(
        seed ^ static_cast<uint32_t>(id) ^ Hash(static_cast<uint32_t>(id >> 32u)) ^ Hash(stream * 0x9E3779B9u)
    );
    return static_cast<float>(value & 0x00FFFFFFu) / 16777216.0f;
}

float3 ParticleSystem::Random3(uint32_t seed, uint64_t id, uint32_t stream)
{
    return {
        Random01(seed, id, stream),
        Random01(seed, id, stream + 1u),
        Random01(seed, id, stream + 2u)
    };
}

float3 ParticleSystem::CurlNoise(
    const float3& position,
    uint32_t seed,
    const Particles::NoiseModule& settings,
    float time
)
{
    float3 result(0.0f);
    float amplitude = 1.0f;
    float frequency = settings.frequency;
    float totalAmplitude = 0.0f;
    const float3 animatedPosition = position + settings.scrollVelocity * time;
    for (int octave = 0; octave < settings.octaves; ++octave)
    {
        result += CurlNoiseOctave(animatedPosition * frequency, seed + static_cast<uint32_t>(octave) * 1013u) *
                  amplitude;
        totalAmplitude += amplitude;
        amplitude *= settings.octaveStrengthMultiplier;
        frequency *= settings.octaveFrequencyMultiplier;
    }
    return totalAmplitude > 0.0f ? result / totalAmplitude : float3(0.0f);
}
