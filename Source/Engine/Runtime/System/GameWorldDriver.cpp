#include "GameWorldDriver.hpp"

void GameWorldDriver::Update()
{
    desireDriver->Update(worldEnvironmentQuery.get());

    // push desires to action processor
    auto actionQueues = desireDriver->GetQueuedActions();
    while (!actionQueues.empty())
    {
        soulActionProcessor->Enqueue(std::move(actionQueues.back()));
        actionQueues.pop_back();
    }

    soulActionProcessor->Update(worldEnvironmentQuery.get());
}
