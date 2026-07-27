#pragma once

#include "Component.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ParticleRenderer.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"

#include "Engine/Shaders/Particles/Particle.hlsl"

#include <cstdint>
#include <limits>
#include <vector>

class Camera;
class Mesh;

namespace Particles
{
enum class CurveInterpolation
{
    Constant,
    Linear,
    Cubic
};

struct CurveKey
{
    float time = 0.0f;
    float value = 0.0f;
    float inTangent = 0.0f;
    float outTangent = 0.0f;
    CurveInterpolation interpolation = CurveInterpolation::Linear;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct Curve
{
    std::vector<CurveKey> keys;

    WEILAN_ENGINE_API explicit Curve(float value = 1.0f);
    WEILAN_ENGINE_API float Evaluate(float normalizedTime) const;
    WEILAN_ENGINE_API void SortKeys();
    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

enum class ScalarMode
{
    Constant,
    RandomBetweenConstants,
    Curve,
    RandomBetweenCurves
};

struct ScalarParameter
{
    ScalarMode mode = ScalarMode::Constant;
    float constant = 0.0f;
    float constantMin = 0.0f;
    float constantMax = 0.0f;
    Curve curveMin;
    Curve curveMax;

    WEILAN_ENGINE_API explicit ScalarParameter(float value = 0.0f);
    WEILAN_ENGINE_API float Evaluate(float normalizedTime, float random) const;
    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct VectorParameter
{
    bool separateAxes = false;
    ScalarParameter x;
    ScalarParameter y;
    ScalarParameter z;

    WEILAN_ENGINE_API explicit VectorParameter(float value = 0.0f);
    WEILAN_ENGINE_API explicit VectorParameter(const float3& value);
    WEILAN_ENGINE_API float3 Evaluate(float normalizedTime, const float3& random) const;
    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct GradientColorKey
{
    float time = 0.0f;
    float3 color = float3(1.0f);

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct GradientAlphaKey
{
    float time = 0.0f;
    float alpha = 1.0f;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct Gradient
{
    std::vector<GradientColorKey> colorKeys;
    std::vector<GradientAlphaKey> alphaKeys;

    WEILAN_ENGINE_API Gradient();
    WEILAN_ENGINE_API explicit Gradient(const float4& color);
    WEILAN_ENGINE_API float4 Evaluate(float normalizedTime) const;
    WEILAN_ENGINE_API void SortKeys();
    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

enum class ColorMode
{
    Color,
    RandomBetweenColors,
    Gradient,
    RandomBetweenGradients
};

struct ColorParameter
{
    ColorMode mode = ColorMode::Color;
    float4 color = float4(1.0f);
    float4 colorMin = float4(1.0f);
    float4 colorMax = float4(1.0f);
    Gradient gradientMin;
    Gradient gradientMax;

    WEILAN_ENGINE_API float4 Evaluate(float normalizedTime, float random) const;
    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

enum class SimulationSpace
{
    World,
    Local
};

enum class CullingMode
{
    AlwaysSimulate,
    PauseWhenCulled
};

enum class ShapeType
{
    Point,
    Disk,
    Rectangle,
    Box,
    Sphere,
    Hemisphere,
    Cone
};

enum class ShapeSampling
{
    Random,
    PoissonDisk
};

enum class ShapeDirection
{
    Axis,
    Outward,
    Random
};

enum class RendererMode
{
    Billboard,
    Mesh
};

enum class BillboardMode
{
    CameraFacing,
    Vertical,
    Horizontal,
    Stretched
};

enum class SortMode
{
    None,
    Distance,
    YoungestFirst,
    OldestFirst
};

enum class PlaybackState
{
    Stopped,
    Playing,
    Paused
};

struct MainModule
{
    float duration = 5.0f;
    bool looping = true;
    bool prewarm = false;
    ScalarParameter startDelay{0.0f};
    ScalarParameter startLifetime{1.0f};
    ScalarParameter startSpeed{1.0f};
    VectorParameter startSize{1.0f};
    VectorParameter startRotation{0.0f};
    ColorParameter startColor;
    ScalarParameter gravityModifier{0.0f};
    SimulationSpace simulationSpace = SimulationSpace::World;
    CullingMode cullingMode = CullingMode::AlwaysSimulate;
    float simulationSpeed = 1.0f;
    bool fixedTimeStep = false;
    float fixedDeltaTime = 1.0f / 60.0f;
    bool playOnAwake = true;
    int maxParticles = 1024;
    bool automaticSeed = true;
    uint32_t seed = 1;
    bool automaticBounds = true;
    AABB manualBounds{float3(-5.0f), float3(5.0f)};

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct Burst
{
    float time = 0.0f;
    int count = 10;
    int cycles = 1;
    float interval = 0.1f;
    float probability = 1.0f;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct EmissionModule
{
    bool enabled = true;
    ScalarParameter rateOverTime{10.0f};
    ScalarParameter rateOverDistance{0.0f};
    std::vector<Burst> bursts;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct ShapeModule
{
    bool enabled = true;
    ShapeType shape = ShapeType::Point;
    ShapeSampling sampling = ShapeSampling::Random;
    ShapeDirection direction = ShapeDirection::Axis;
    float3 axis = float3(0.0f, 1.0f, 0.0f);
    float3 rotation = float3(0.0f);
    float radius = 1.0f;
    float3 size = float3(1.0f);
    float coneAngle = 25.0f;
    float coneLength = 1.0f;
    bool shell = false;
    float poissonMinDistance = 0.1f;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct VelocityOverLifetimeModule
{
    bool enabled = false;
    VectorParameter velocity{0.0f};
    ScalarParameter speedMultiplier{1.0f};
    float inheritEmitterVelocity = 0.0f;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct ColorOverLifetimeModule
{
    bool enabled = false;
    ColorParameter color;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct SizeOverLifetimeModule
{
    bool enabled = false;
    VectorParameter size{1.0f};

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct RotationOverLifetimeModule
{
    bool enabled = false;
    VectorParameter angularVelocity{0.0f};

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct NoiseModule
{
    bool enabled = false;
    VectorParameter strength{1.0f};
    float frequency = 0.5f;
    float3 scrollVelocity = float3(0.0f);
    int octaves = 1;
    float octaveStrengthMultiplier = 0.5f;
    float octaveFrequencyMultiplier = 2.0f;
    float damping = 0.0f;
    SimulationSpace space = SimulationSpace::World;
    float positionInfluence = 1.0f;
    float rotationInfluence = 0.0f;
    float sizeInfluence = 0.0f;
    uint32_t seedOffset = 0;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct LimitVelocityModule
{
    bool enabled = false;
    ScalarParameter speedLimit{10.0f};
    ScalarParameter drag{0.0f};
    float dampen = 1.0f;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct TextureSheetModule
{
    bool enabled = false;
    int columns = 1;
    int rows = 1;
    ScalarParameter frameOverLifetime{0.0f};
    float cycles = 1.0f;
    bool randomStartFrame = false;

    TextureSheetModule();

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct RendererModule
{
    RendererMode mode = RendererMode::Billboard;
    BillboardMode billboardMode = BillboardMode::CameraFacing;
    SortMode sortMode = SortMode::Distance;
    ObjPtr<Material> material = nullptr;
    ObjPtr<Mesh> mesh = nullptr;
    float2 pivot = float2(0.0f);
    float stretchScale = 1.0f;
    float velocityScale = 0.0f;
    float sortBias = 0.0f;
    float minScreenSize = 0.0f;
    float maxScreenSize = 1.0f;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct PerformanceSettings
{
    bool multithreadedSimulation = true;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct PreviewSettings
{
    bool showShape = true;
    bool showBounds = true;
    int shapeSampleCount = 32;
    float sampleMarkerSize = 0.03f;

    void Serialize(Serializer* s) const;
    void Deserialize(Serializer* s);
};

struct SimulationStats
{
    int activeParticles = 0;
    int emittedThisFrame = 0;
    int droppedParticles = 0;
    int scheduledJobs = 0;
    float simulationMilliseconds = 0.0f;
    float sortingMilliseconds = 0.0f;
    float uploadMilliseconds = 0.0f;
    bool visible = true;
    bool capacitySaturated = false;
};
} // namespace Particles

class ParticleSystem : public Component
{
    DECLARE_OBJECT();

public:
    WEILAN_ENGINE_API ParticleSystem();
    WEILAN_ENGINE_API explicit ParticleSystem(GameObject* gameObject);
    WEILAN_ENGINE_API ~ParticleSystem() override;
    const std::string& GetName() const override;

    void OnEnable() override;
    void OnDisable() override;
    void OnAwake() override;
    void Tick() override;
    void IdleTick() override;
    void DebugDraw() override;
    void OnDrawGizmos() override;

    void Serialize(Serializer* ser) const override;
    void Deserialize(Serializer* des) override;

    WEILAN_ENGINE_API void Play();
    WEILAN_ENGINE_API void Pause();
    WEILAN_ENGINE_API void Stop();
    WEILAN_ENGINE_API void Restart();
    WEILAN_ENGINE_API void Simulate(float deltaTime);
    WEILAN_ENGINE_API void SetParticleCount(int count);
    WEILAN_ENGINE_API void NotifySettingsChanged();

    Particles::PlaybackState GetPlaybackState() const { return playbackState; }
    float GetSimulationTime() const { return systemTime; }
    int GetParticleCount() const { return main.maxParticles; }
    int GetActiveParticleCount() const { return stats.activeParticles; }

    Particles::MainModule& GetMainModule() { return main; }
    Particles::EmissionModule& GetEmissionModule() { return emission; }
    Particles::ShapeModule& GetShapeModule() { return shape; }
    Particles::VelocityOverLifetimeModule& GetVelocityOverLifetimeModule() { return velocityOverLifetime; }
    Particles::ColorOverLifetimeModule& GetColorOverLifetimeModule() { return colorOverLifetime; }
    Particles::SizeOverLifetimeModule& GetSizeOverLifetimeModule() { return sizeOverLifetime; }
    Particles::RotationOverLifetimeModule& GetRotationOverLifetimeModule() { return rotationOverLifetime; }
    Particles::NoiseModule& GetNoiseModule() { return noise; }
    Particles::LimitVelocityModule& GetLimitVelocityModule() { return limitVelocity; }
    Particles::TextureSheetModule& GetTextureSheetModule() { return textureSheet; }
    Particles::RendererModule& GetRendererModule() { return renderer; }
    Particles::PerformanceSettings& GetPerformanceSettings() { return performance; }
    Particles::PreviewSettings& GetPreviewSettings() { return preview; }
    const Particles::SimulationStats& GetSimulationStats() const { return stats; }

    void SetParticleMesh(Mesh* mesh);
    Mesh* GetParticleMesh() const { return renderer.mesh.Get(); }

    Rendering::ParticleDraw GetDraw(Camera& camera);
    WEILAN_ENGINE_API std::vector<float3> GenerateShapePreviewSamples(size_t count);
    WEILAN_ENGINE_API const AABB& GetBounds() const;
    bool IsVisible(const Frustum& frustum) const;
    void SetVisible(bool visible) { stats.visible = visible; }

private:
    struct Storage
    {
        std::vector<float3> positions;
        std::vector<float3> velocities;
        std::vector<float3> startSizes;
        std::vector<float3> sizes;
        std::vector<glm::quat> rotations;
        std::vector<float4> startColors;
        std::vector<float4> colors;
        std::vector<float> ages;
        std::vector<float> lifetimes;
        std::vector<float> textureFrames;
        std::vector<uint32_t> randomSeeds;
        std::vector<uint64_t> spawnIds;
        std::vector<int32_t> poissonSampleIndices;
        std::vector<uint8_t> alive;
        std::vector<uint32_t> activeSlots;
        std::vector<uint32_t> freeSlots;

        void Resize(size_t capacity);
        void Clear();
    };

    struct ChunkResult
    {
        float3 min = float3(std::numeric_limits<float>::max());
        float3 max = float3(std::numeric_limits<float>::lowest());
        bool hasBounds = false;
    };

    static constexpr int CurrentVersion = 1;
    static constexpr size_t ParallelParticleThreshold = 512;

    int particleSystemVersion = CurrentVersion;
    Particles::MainModule main;
    Particles::EmissionModule emission;
    Particles::ShapeModule shape;
    Particles::VelocityOverLifetimeModule velocityOverLifetime;
    Particles::ColorOverLifetimeModule colorOverLifetime;
    Particles::SizeOverLifetimeModule sizeOverLifetime;
    Particles::RotationOverLifetimeModule rotationOverLifetime;
    Particles::NoiseModule noise;
    Particles::LimitVelocityModule limitVelocity;
    Particles::TextureSheetModule textureSheet;
    Particles::RendererModule renderer;
    Particles::PerformanceSettings performance;
    Particles::PreviewSettings preview;
    Particles::SimulationStats stats;

    Particles::PlaybackState playbackState = Particles::PlaybackState::Stopped;
    Storage storage;
    float systemTime = 0.0f;
    float previousSystemTime = 0.0f;
    float startDelayRemaining = 0.0f;
    float fixedTimeAccumulator = 0.0f;
    float timeEmissionAccumulator = 0.0f;
    float distanceEmissionAccumulator = 0.0f;
    uint64_t nextSpawnId = 1;
    uint64_t loopIndex = 0;
    uint32_t runtimeSeed = 1;
    float3 previousEmitterPosition = float3(0.0f);
    float3 emitterVelocity = float3(0.0f);
    std::vector<float> nextBurstTimes;
    std::vector<int> remainingBurstCycles;
    std::vector<float3> poissonSamples;
    std::vector<uint32_t> freePoissonSampleIndices;
    size_t poissonSignature = 0;

    std::vector<uint8_t> deathFlags;
    std::vector<GPUParticle> renderParticles;
    std::vector<GPUParticle> sortedRenderParticles;
    std::vector<ChunkResult> chunkResults;
    AABB automaticBounds;

    Rendering::ParticleDraw draw;
    std::unique_ptr<Gfx::Buffer> particleBuffer;
    std::unique_ptr<Material> particleMaterial;
    bool materialDirty = true;
    bool gpuStateInitialized = false;

    void InitializeStorage();
    void InitializeGpuState();
    void RefreshMaterial();
    void ResetBurstState();
    void SimulateStep(float deltaTime);
    int CalculateEmission(float deltaTime);
    void SpawnParticles(int count);
    void SpawnParticle(uint32_t slot);
    void SimulateRange(
        size_t begin,
        size_t end,
        float deltaTime,
        const float4x4& simulationToWorld,
        const float4x4& emitterToWorld,
        ChunkResult& result
    );
    void ReclaimDeadParticles();
    void UpdateBounds();
    void PrepareRenderData(Camera& camera);
    float3 GetEmitterPosition() const;
    float4x4 GetEmitterWorldMatrix() const;

    float3 SampleShape(uint32_t slot, uint64_t spawnId);
    float3 SampleRandomShape(uint64_t spawnId) const;
    float3 SampleDirection(const float3& position, uint64_t spawnId) const;
    void EnsurePoissonSamples();
    size_t CalculateShapeSignature() const;

    static uint32_t Hash(uint32_t value);
    static float Random01(uint32_t seed, uint64_t id, uint32_t stream);
    static float3 Random3(uint32_t seed, uint64_t id, uint32_t stream);
    static float3 CurlNoise(const float3& position, uint32_t seed, const Particles::NoiseModule& settings, float time);
};
