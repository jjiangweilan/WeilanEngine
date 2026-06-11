#include "Engine/Runtime/Object/Component/ParticleSystem.hpp"
#include <gtest/gtest.h>

TEST(ParticleSystemTest, ClampsNegativeCountBeforeAwake)
{
    ParticleSystem particleSystem;

    EXPECT_NO_THROW(particleSystem.SetParticleCount(-100));
    EXPECT_EQ(particleSystem.GetParticleCount(), 1);
}

TEST(ParticleSystemTest, ResizesCountBeforeAwakeWithoutGpuState)
{
    ParticleSystem particleSystem;

    EXPECT_NO_THROW(particleSystem.SetParticleCount(32));
    EXPECT_EQ(particleSystem.GetParticleCount(), 32);
    EXPECT_NO_THROW(particleSystem.SetParticleCount(0));
    EXPECT_EQ(particleSystem.GetParticleCount(), 1);
}
