#include "UID64.hpp"
static std::mt19937_64 CreateRandomGenerator()
{
    std::random_device dev;
    std::mt19937_64 rng(dev());
    return rng;
}

std::mt19937_64 UID64::rng = CreateRandomGenerator();

std::uniform_int_distribution<std::mt19937_64::result_type> UID64::dist =
    std::uniform_int_distribution<std::mt19937_64::result_type>(0, std::numeric_limits<uint64_t>::max());
