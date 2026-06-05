#pragma once
#include "SoulAction.hpp"
#include <vector>

class WorldEnvironmentQuery;
namespace Soul
{
class ActionProcessor
{
public:
    void Enqueue(std::unique_ptr<Action>&& action);
    void Update(WorldEnvironmentQuery* worldEnvironmentQuery);

private:
    std::vector<std::unique_ptr<Action>> queuedActions = {};
    std::vector<std::unique_ptr<Action>> activeActions = {};
};
} // namespace Soul
