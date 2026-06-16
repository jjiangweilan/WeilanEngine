#include "AnimationSet.hpp"

DEFINE_ASSET(AnimationSet, "2389367B-26D5-4E6D-AE95-E81C8D4C741A", "animset")

void AnimationSet::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("clips", clips);
}

void AnimationSet::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("clips", clips);
}
