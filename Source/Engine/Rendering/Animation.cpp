#include "Animation.hpp"

DEFINE_ASSET(Animation, "8BC62722-3891-4A4D-866B-96A35CE7C030", "anim")

Animation::Channel::Channel() : positions(), rotations(), scalings() {}
Animation::Channel::Channel(Channel&& other)
{
    positions = std::move(other.positions);
    rotations = std::move(other.rotations);
    scalings = std::move(other.scalings);
}
Animation::Channel::Channel(const Channel& other)
{
    positions = other.positions;
    rotations = other.rotations;
    scalings = other.scalings;
}

Animation::Animation() {}



