#include "WeilanEngine.hpp"
#include <gtest/gtest.h>
using namespace Engine;
TEST(EnvironmentBaker, Test0)
{
    Engine::WeilanEngine engine;
    engine.Init({});

    auto& gfxDriver = engine.gfxDriver;
}
