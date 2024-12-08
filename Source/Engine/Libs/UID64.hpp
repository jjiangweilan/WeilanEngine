#pragma once
#include <random>

// a 64 bit ID from a random generator
class UID64
{
public:
    UID64() = default;
    UID64(const UID64& other) = default;
    UID64& operator=(const UID64& other) = default;
    UID64(uint64_t id) : id(id) {}
    operator uint64_t() const { return id; }
    bool operator==(const UID64& other) const = default;
    bool operator!=(const UID64& other) const = default;

private:
    static std::uniform_int_distribution<std::mt19937_64::result_type> dist;
    static std::mt19937_64 rng;

    uint64_t id = dist(rng);
};
