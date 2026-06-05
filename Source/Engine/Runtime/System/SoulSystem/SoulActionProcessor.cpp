#include "SoulActionProcessor.hpp"

#include <algorithm>

namespace Soul
{

void ActionProcessor::Enqueue(std::unique_ptr<Action>&& action)
{
    queuedActions.push_back(std::move(action));
}

void ActionProcessor::Update(WorldEnvironmentQuery* worldEnvironmentQuery)
{
    std::stable_sort(
        queuedActions.begin(),
        queuedActions.end(),
        [](const std::unique_ptr<Action>& lhs, const std::unique_ptr<Action>& rhs)
        {
            return lhs->GetPriority() > rhs->GetPriority();
        }
    );

    for (size_t i = 0; i < queuedActions.size();)
    {
        Action* action = queuedActions[i].get();
        const auto& constraints = action->GetConstraints();
        const bool allSatisfied = std::all_of(
            constraints.begin(),
            constraints.end(),
            [](const std::unique_ptr<ActionConstraint>& constraint)
            {
                return constraint->IsSatisfied();
            }
        );

        if (allSatisfied)
        {
            action->OnStart();
            activeActions.push_back(std::move(queuedActions[i]));
            queuedActions.erase(queuedActions.begin() + i);
        }
        else
        {
            ++i;
        }
    }

    for (size_t i = 0; i < activeActions.size();)
    {
        Action* action = activeActions[i].get();
        action->Update();

        if (action->IsFinished())
        {
            action->OnEnd();
            activeActions.erase(activeActions.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}
} // namespace Soul
