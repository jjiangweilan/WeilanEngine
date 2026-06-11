#pragma once
#include "Component.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ParticleRenderer.hpp"

#include "Engine/Shaders/Particles/Particle.hlsl"

class ParticleSystem;

struct Particle
{
    float3 position = {0, 0, 0};
    float3 scale = {1, 1, 1};
    glm::quat rotation = {0, 0, 0, 1};
    float3 velocity = {0, 0, 0};
    float4 color = {1, 1, 1, 1};

    float lifetime = 1;
    int aliveFrames = 0;
    bool active = false;

    ParticleSystem* system;

    bool IsActive() { return active; }
    void Activate();
    void Deactivate()
    {
        aliveFrames = -1;
        active = false;
    }
};

namespace ParticleModifiers
{

struct UpdateInfo
{
    ParticleSystem* root;
    float deltaTime;
    float totalTime;
};

enum class ModifierType
{
    Velocity,
    Emitter,
    Fade
};

class Base : public Object
{
public:
    virtual void OnParticleActivate(Particle& particle) {}
    virtual void Update(Particle& particle, int idx, UpdateInfo& updateInfo) = 0;
    virtual void BeforeUpdate(float deltaTime) {}
    virtual void AfterUpdate(float deltaTime) {}
};

class Fade : public Base
{
    DECLARE_OBJECT();

public:
    virtual void OnParticleActivate(Particle& particle) override;
    virtual void Update(Particle& particle, int idx, UpdateInfo& updateInfo) override;
    virtual void Serialize(Serializer* s) const override { SERIALIZE(s, fadeSpeed); }
    virtual void Deserialize(Serializer* s) override { DESERIALIZE(s, fadeSpeed); }

    float fadeSpeed = 1.0f;

private:
};

class Velocity : public Base
{
    DECLARE_OBJECT();

public:
    virtual void OnParticleActivate(Particle& particle) override;
    virtual void Update(Particle& particle, int idx, UpdateInfo& updateInfo) override;

    // Serialization
    virtual void Serialize(Serializer* s) const override
    {
        SERIALIZE(s, dir);
        SERIALIZE(s, minSpeed);
        SERIALIZE(s, maxSpeed);
        SERIALIZE(s, initialSpeed);
        SERIALIZE(s, acceleration);
    }

    virtual void Deserialize(Serializer* s) override
    {
        DESERIALIZE(s, dir);
        DESERIALIZE(s, minSpeed);
        DESERIALIZE(s, maxSpeed);
        DESERIALIZE(s, initialSpeed);
        DESERIALIZE(s, acceleration);
    }

    Velocity& SetAcceleration(float acceleration)
    {
        this->acceleration = acceleration;
        return *this;
    }

    Velocity& SetDir(float3 dir)
    {
        this->dir = dir;
        return *this;
    }

    Velocity& SetInitialSpeed(float speed)
    {
        this->initialSpeed = speed;
        return *this;
    }

private:
    float3 dir = {0, 1, 0};
    float3 minSpeed = glm::float3{
        std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()
    };
    float3 maxSpeed = glm::float3{
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()
    };
    float initialSpeed = 0.01f;
    float acceleration = 0.0f;
};

class Emitter : public Base
{
    DECLARE_OBJECT();

public:
    void OnParticleActivate(Particle& particle) override;

    void Serialize(Serializer* s) const override
    {
        SERIALIZE(s, emitCountPerFrame);
        SERIALIZE(s, emitInterval);
        SERIALIZE(s, restart);
    }

    void Deserialize(Serializer* s) override
    {
        DESERIALIZE(s, emitCountPerFrame);
        DESERIALIZE(s, emitInterval);
        DESERIALIZE(s, restart);
    }

    Emitter& SetEmitInterval(float interval)
    {
        this->emitInterval = interval;
        return *this;
    }

    Emitter& GetEmitCount(int count)
    {
        this->emitCountPerFrame = count;
        return *this;
    }

    Emitter& SetRestart(bool restart)
    {
        this->restart = restart;
        return *this;
    }

    void Update(Particle& particle, int idx, UpdateInfo& updateInfo) override;
    void BeforeUpdate(float deltaTime) override;
    void AfterUpdate(float deltaTime) override;

private:
    bool restart = false;
    int emitCountPerFrame = 1;
    float emitInterval = 1.0f;

    float timePassedSinceEmit = 0.0f;
    int emittedCount = 0;
    bool emitForThisFrame = false;
};
}; // namespace ParticleModifiers

class ParticleSystem : public Component
{
    DECLARE_OBJECT();

public:
    WEILAN_ENGINE_API ParticleSystem();
    WEILAN_ENGINE_API explicit ParticleSystem(GameObject* gameObject);
    WEILAN_ENGINE_API ~ParticleSystem() override;
    const std::string& GetName() const override;

    void OnEnable() override;
    void OnDisable() override;

    void Serialize(Serializer* ser) const override;
    void Deserialize(Serializer* des) override;

    void OnAwake() override;
    void Tick() override;
    void IdleTick() override;

    template <class T>
    T& AddParticleModifier()
    {
        auto ptr = std::make_unique<T>();
        auto tmp = ptr.get();
        particleModifiers.push_back(std::move(ptr));
        return *tmp;
    }

    void AddParticleModifier(std::unique_ptr<ParticleModifiers::Base>&& modifier)
    {
        particleModifiers.push_back(std::move(modifier));
    }
    void SetParticleMesh(Mesh* mesh) { this->particleMesh = mesh; }
    WEILAN_ENGINE_API void SetParticleCount(int count);
    Mesh* GetParticleMesh() { return this->particleMesh; }
    int GetParticleCount() { return this->particleCount; }

    void AddParticleModifier();

    Rendering::ParticleDraw GetDraw();

private:
    float totalTime = .0f;
    Rendering::ParticleDraw draw;
    Mesh* particleMesh = nullptr;
    int particleCount = 1024;
    std::vector<Particle> particles;
    std::unique_ptr<Gfx::Buffer> particleWorldMatrixBuffer;
    std::unique_ptr<Material> particleParameters;
    std::vector<std::unique_ptr<ParticleModifiers::Base>> particleModifiers;
    size_t GetParticleBufferByteSize(int particleCount);
    void ResizeParticleStorage();
    void UpdatePositionBuffer();

    void OnParticleActivate(Particle& particle);
    friend class Particle;
};
