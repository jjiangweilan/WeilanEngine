#include "Random.hpp"

std::mt19937& Random::GetThreadLocalMT19937()
{
    thread_local std::mt19937 mt(std::random_device{}());
    return mt;
}

std::uniform_real_distribution<float> Random::GetThreadLocalUniformRealDistribution()
{
    thread_local std::uniform_real_distribution<float> d;
    return d;
}

float Random::GenerateNormalized()
{
    return GetThreadLocalUniformRealDistribution()(GetThreadLocalMT19937());
}
