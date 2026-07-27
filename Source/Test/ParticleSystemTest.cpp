#include "Engine/Core/JobSystem.hpp"
#include "Engine/Runtime/Object/Component/ParticleSystem.hpp"

#include <gtest/gtest.h>

namespace
{
void ConfigureForCpuTest(ParticleSystem& particleSystem, int capacity = 128)
{
    auto& main = particleSystem.GetMainModule();
    main.maxParticles = capacity;
    main.duration = 10.0f;
    main.looping = true;
    main.automaticSeed = false;
    main.seed = 12345;
    main.startLifetime = Particles::ScalarParameter(10.0f);
    main.startSpeed = Particles::ScalarParameter(0.0f);
    main.gravityModifier = Particles::ScalarParameter(0.0f);
    particleSystem.GetPerformanceSettings().multithreadedSimulation = false;
    particleSystem.NotifySettingsChanged();
}

void ConfigureNoiseSystem(ParticleSystem& particleSystem, uint32_t seed)
{
    ConfigureForCpuTest(particleSystem, 64);
    particleSystem.GetMainModule().seed = seed;
    particleSystem.GetEmissionModule().rateOverTime = Particles::ScalarParameter(20.0f);
    particleSystem.GetShapeModule().shape = Particles::ShapeType::Point;

    auto& noise = particleSystem.GetNoiseModule();
    noise.enabled = true;
    noise.strength = Particles::VectorParameter(float3(2.0f));
    noise.frequency = 0.7f;
    noise.octaves = 3;
    noise.octaveStrengthMultiplier = 0.55f;
    noise.octaveFrequencyMultiplier = 2.1f;
    particleSystem.NotifySettingsChanged();
}
} // namespace

TEST(ParticleSystemTest, ClampsParticleCapacityBeforeAwake)
{
    ParticleSystem particleSystem;

    EXPECT_NO_THROW(particleSystem.SetParticleCount(-100));
    EXPECT_EQ(particleSystem.GetParticleCount(), 1);
    EXPECT_NO_THROW(particleSystem.SetParticleCount(32));
    EXPECT_EQ(particleSystem.GetParticleCount(), 32);
}

TEST(ParticleSystemTest, LinearCurveInterpolatesAndClamps)
{
    Particles::Curve curve;
    curve.keys = {{0.0f, 2.0f}, {1.0f, 6.0f}};

    EXPECT_FLOAT_EQ(curve.Evaluate(-1.0f), 2.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f), 4.0f);
    EXPECT_FLOAT_EQ(curve.Evaluate(2.0f), 6.0f);
}

TEST(ParticleSystemTest, GradientInterpolatesColorAndAlpha)
{
    Particles::Gradient gradient;
    gradient.colorKeys = {{0.0f, float3(1.0f, 0.0f, 0.0f)}, {1.0f, float3(0.0f, 0.0f, 1.0f)}};
    gradient.alphaKeys = {{0.0f, 0.0f}, {1.0f, 1.0f}};

    const float4 value = gradient.Evaluate(0.5f);
    EXPECT_FLOAT_EQ(value.r, 0.5f);
    EXPECT_FLOAT_EQ(value.g, 0.0f);
    EXPECT_FLOAT_EQ(value.b, 0.5f);
    EXPECT_FLOAT_EQ(value.a, 0.5f);
}

TEST(ParticleSystemTest, RandomDiskPreviewSamplesStayInsideDisk)
{
    ParticleSystem particleSystem;
    auto& shape = particleSystem.GetShapeModule();
    shape.shape = Particles::ShapeType::Disk;
    shape.radius = 3.0f;
    shape.sampling = Particles::ShapeSampling::Random;

    const auto samples = particleSystem.GenerateShapePreviewSamples(256);
    ASSERT_EQ(samples.size(), 256);
    for (const float3& point : samples)
    {
        EXPECT_FLOAT_EQ(point.y, 0.0f);
        EXPECT_LE(point.x * point.x + point.z * point.z, 9.00001f);
    }
}

TEST(ParticleSystemTest, RandomRectanglePreviewSamplesStayInsideRectangle)
{
    ParticleSystem particleSystem;
    auto& shape = particleSystem.GetShapeModule();
    shape.shape = Particles::ShapeType::Rectangle;
    shape.size = float3(4.0f, 0.0f, 2.0f);

    const auto samples = particleSystem.GenerateShapePreviewSamples(256);
    ASSERT_EQ(samples.size(), 256);
    for (const float3& point : samples)
    {
        EXPECT_FLOAT_EQ(point.y, 0.0f);
        EXPECT_LE(std::abs(point.x), 2.0f);
        EXPECT_LE(std::abs(point.z), 1.0f);
    }
}

TEST(ParticleSystemTest, PoissonDiskPreviewSamplesRespectMinimumDistance)
{
    constexpr size_t sampleCount = 64;
    constexpr float minimumDistance = 0.5f;

    ParticleSystem particleSystem;
    auto& shape = particleSystem.GetShapeModule();
    shape.shape = Particles::ShapeType::Disk;
    shape.radius = 5.0f;
    shape.sampling = Particles::ShapeSampling::PoissonDisk;
    shape.poissonMinDistance = minimumDistance;

    const auto samples = particleSystem.GenerateShapePreviewSamples(sampleCount);
    ASSERT_EQ(samples.size(), sampleCount);
    for (size_t i = 0; i < samples.size(); ++i)
    {
        for (size_t j = i + 1; j < samples.size(); ++j)
            EXPECT_GE(glm::distance2(samples[i], samples[j]), minimumDistance * minimumDistance);
    }
}

TEST(ParticleSystemTest, TimeEmissionAndCapacityStatsAreStable)
{
    ParticleSystem particleSystem;
    ConfigureForCpuTest(particleSystem, 3);
    particleSystem.GetEmissionModule().rateOverTime = Particles::ScalarParameter(100.0f);

    particleSystem.Play();
    particleSystem.Simulate(0.1f);

    EXPECT_EQ(particleSystem.GetActiveParticleCount(), 3);
    EXPECT_EQ(particleSystem.GetSimulationStats().emittedThisFrame, 3);
    EXPECT_EQ(particleSystem.GetSimulationStats().droppedParticles, 7);
    EXPECT_TRUE(particleSystem.GetSimulationStats().capacitySaturated);
}

TEST(ParticleSystemTest, LivePoissonSamplesAreNotReused)
{
    ParticleSystem particleSystem;
    ConfigureForCpuTest(particleSystem, 32);
    auto& shape = particleSystem.GetShapeModule();
    shape.shape = Particles::ShapeType::Disk;
    shape.radius = 1.0f;
    shape.sampling = Particles::ShapeSampling::PoissonDisk;
    shape.poissonMinDistance = 1.0f;
    particleSystem.GetEmissionModule().rateOverTime = Particles::ScalarParameter(320.0f);
    particleSystem.NotifySettingsChanged();

    particleSystem.Play();
    particleSystem.Simulate(0.1f);

    EXPECT_GT(particleSystem.GetActiveParticleCount(), 0);
    EXPECT_LT(particleSystem.GetActiveParticleCount(), 32);
    EXPECT_EQ(
        particleSystem.GetSimulationStats().droppedParticles,
        32 - particleSystem.GetActiveParticleCount()
    );
}

TEST(ParticleSystemTest, StopClearsParticlesAndResetsTime)
{
    ParticleSystem particleSystem;
    ConfigureForCpuTest(particleSystem);
    particleSystem.GetEmissionModule().rateOverTime = Particles::ScalarParameter(20.0f);

    particleSystem.Play();
    particleSystem.Simulate(0.25f);
    ASSERT_EQ(particleSystem.GetActiveParticleCount(), 5);

    particleSystem.Stop();
    EXPECT_EQ(particleSystem.GetPlaybackState(), Particles::PlaybackState::Stopped);
    EXPECT_EQ(particleSystem.GetActiveParticleCount(), 0);
    EXPECT_FLOAT_EQ(particleSystem.GetSimulationTime(), 0.0f);
}

TEST(ParticleSystemTest, NonLoopingSystemStopsAfterLastParticleDies)
{
    ParticleSystem particleSystem;
    ConfigureForCpuTest(particleSystem);
    auto& main = particleSystem.GetMainModule();
    main.duration = 0.2f;
    main.looping = false;
    main.startLifetime = Particles::ScalarParameter(0.1f);
    particleSystem.GetEmissionModule().rateOverTime = Particles::ScalarParameter(10.0f);

    particleSystem.Play();
    particleSystem.Simulate(0.1f);
    particleSystem.Simulate(0.1f);

    EXPECT_EQ(particleSystem.GetActiveParticleCount(), 0);
    EXPECT_EQ(particleSystem.GetPlaybackState(), Particles::PlaybackState::Stopped);
}

TEST(ParticleSystemTest, NoiseIsDeterministicForASeed)
{
    ParticleSystem first;
    ParticleSystem second;
    ConfigureNoiseSystem(first, 77);
    ConfigureNoiseSystem(second, 77);

    first.Play();
    second.Play();
    for (int i = 0; i < 10; ++i)
    {
        first.Simulate(0.1f);
        second.Simulate(0.1f);
    }

    const AABB& firstBounds = first.GetBounds();
    const AABB& secondBounds = second.GetBounds();
    EXPECT_EQ(first.GetActiveParticleCount(), second.GetActiveParticleCount());
    EXPECT_EQ(firstBounds.min, secondBounds.min);
    EXPECT_EQ(firstBounds.max, secondBounds.max);
}

TEST(ParticleSystemTest, LargeSimulationSchedulesAndCompletesJobs)
{
    JobSystem::InitJobSystem();

    ParticleSystem particleSystem;
    ConfigureForCpuTest(particleSystem, 600);
    particleSystem.GetPerformanceSettings().multithreadedSimulation = true;
    particleSystem.GetEmissionModule().rateOverTime = Particles::ScalarParameter(600.0f);
    particleSystem.Play();
    particleSystem.Simulate(1.0f);

    EXPECT_EQ(particleSystem.GetActiveParticleCount(), 600);
    EXPECT_GT(particleSystem.GetSimulationStats().scheduledJobs, 0);

    JobSystem::DeinitJobSystem();
}
