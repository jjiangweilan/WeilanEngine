#pragma once

namespace Particles
{
struct Curve;
}

namespace Editor
{
class CurveModifier
{
public:
    static bool Draw(const char* label, Particles::Curve& curve);
};
} // namespace Editor
