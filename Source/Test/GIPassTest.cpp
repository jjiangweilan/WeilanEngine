#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/GI.hpp"
#include <gtest/gtest.h>

TEST(GIPassTest, AdaptiveRayCountLerpsFromMaxToLow)
{
    EXPECT_EQ(Rendering::Passes::GI::ComputeAdaptiveRayCount(0.0f, 1u, 8u, 4.0f), 8u);
    EXPECT_EQ(Rendering::Passes::GI::ComputeAdaptiveRayCount(1.0f, 1u, 8u, 4.0f), 1u);
    EXPECT_EQ(Rendering::Passes::GI::ComputeAdaptiveRayCount(0.25f, 1u, 8u, 24.0f), 5u);
}

TEST(GIPassTest, AdaptiveRayCountClampsInvalidSettings)
{
    EXPECT_EQ(Rendering::Passes::GI::ComputeAdaptiveRayCount(0.0f, 0u, 0u, 0.0f), 1u);
}
