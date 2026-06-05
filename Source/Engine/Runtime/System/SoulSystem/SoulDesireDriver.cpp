#include "SoulDesireDriver.hpp"
#include "Engine/Core/Time.hpp"

void Soul::DesireDriver::Update(WorldEnvironmentQuery* worldEnvironmentQuery)
{
    for (int i = 0; i < desires.size();)
    {
        auto& desire = desires[i];
        desire.value += desire.increment * Time::DeltaTime();

        if (desire.value > 1)
        {
            if (desire.completeTag == DesireCompleteTag::Remove)
            {
                desires.erase(desires.begin() + i);

                PushTriggerActions(desire);
            }
            else if (desire.completeTag == DesireCompleteTag::Reset)
            {
                i++;
            }
            else
            {
                i++;
            }
        }
    }
}

std::vector<std::unique_ptr<Soul::Action>> Soul::DesireDriver::GetQueuedActions()
{
    auto tmp = std::move(queuedActions);
    queuedActions.clear();
    return tmp;
}

void Soul::DesireDriver::PushTriggerActions(Desire& desire)
{
    queuedActions.insert(queuedActions.end(), desire.triggerActions.begin(), desire.triggerActions.end());
}
