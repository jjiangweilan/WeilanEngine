#pragma once
#include <random>

class Random
{
public:
    static float GenerateNormalized();

    static std::mt19937& GetThreadLocalMT19937();
    static std::uniform_real_distribution<float> GetThreadLocalUniformRealDistribution();
};
