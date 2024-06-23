#pragma once
#include "Component.hpp"
#include "Rendering/GlobalIllumination/LightFieldProbes/LightFieldProbes.hpp"
class LightFieldProbes : public Component
{
    DECLARE_OBJECT();

public:
    LightFieldProbes();
    LightFieldProbes(GameObject* gameObject);
    ~LightFieldProbes() {}
    void SetGrid(const glm::vec3& min, const glm::vec3& max)
    {
        this->gridMin = min;
        this->gridMax = max;
    }

    void GetGrid(glm::vec3& min, glm::vec3& max)
    {
        min = this->gridMin;
        max = this->gridMax;
    }

    void SetProbeCounts(const glm::vec3& count)
    {
        this->probeCount = count;
    }

    const glm::vec3& GetProbeCounts()
    {
        return probeCount;
    }

    void BakeProbeCubemaps()
    {
        lfp.PlaceProbes(gridMin, gridMax, probeCount);
        lfp.CaptureProbeGBuffers();
    }

    std::span<Rendering::LFP::Probe> GetProbes()
    {
        return lfp.GetProbes();
    }

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    Rendering::LFP::LightFieldProbes lfp;
    void EnableImple() override;
    void DisableImple() override;

    glm::vec3 gridMin;
    glm::vec3 gridMax;
    glm::vec3 probeCount;
};
