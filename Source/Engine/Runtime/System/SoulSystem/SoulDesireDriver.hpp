#pragma once

#include "SoulAction.hpp"

#include <memory>
#include <string>
#include <vector>

class WorldEnvironmentQuery;
namespace Soul
{

enum DesireCompleteTag
{
    Remove,
    Reset
};

struct Desire
{
    std::string name = "";
    float value = 0;
    float increment = 0;
    float threshold = 0.8;
    DesireCompleteTag completeTag = DesireCompleteTag::Remove;

    std::vector<std::unique_ptr<Action>> triggerActions = {};
};

class DesireDriver
{
public:
    std::vector<std::unique_ptr<Action>> GetQueuedActions();

    void Update(WorldEnvironmentQuery* worldEnvironmentQuery);

    void AddDesire(const Desire& desire);

private:
    std::vector<Desire> desires = {};
    std::vector<std::unique_ptr<Action>> queuedActions;
    void PushTriggerActions(Desire& desire);
};
} // namespace Soul
