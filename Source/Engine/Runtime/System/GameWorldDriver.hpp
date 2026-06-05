#pragma once
#include "SoulSystem/SoulActionProcessor.hpp"
#include "SoulSystem/SoulDesireDriver.hpp"
#include "WorldEnvironmentQuery/WorldEnvironmentQuery.hpp"

class GameWorldDriver
{
public:
    void Update();

private:
    std::unique_ptr<Soul::DesireDriver> desireDriver;
    std::unique_ptr<WorldEnvironmentQuery> worldEnvironmentQuery;
    std::unique_ptr<Soul::ActionProcessor> soulActionProcessor;
};
