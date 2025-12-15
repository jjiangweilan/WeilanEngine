#include "ParticleSystem.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Core/Time.hpp"

DEFINE_COMPONENT(ParticleSystem, "78E33F89-76E6-4B90-831F-490EB6C9F8D1")

void ParticleSystem::OnInit()
{
    if (particleMesh == nullptr)
    {
        particleMesh = EngineInternalResources::GetModels().sphere;
    }

    particleParameters->SetShader(ShaderLibrary::GetShader(Shaders::Particle));

    UpdatePositionBuffer();
}

void ParticleSystem::OnEnable()
{
    auto scene = GetScene();
    if (scene)
    {
        scene->GetRenderingScene().AddRenderObject(*this);
    }

    AddParticleModifier<ParticleModifiers::Emitter>().SetRestart(true).SetEmitInterval(0.001f);
    AddParticleModifier<ParticleModifiers::Velocity>().SetDir({0, 1, 0}).SetInitialSpeed(1.0f);
    auto& fade = AddParticleModifier<ParticleModifiers::Fade>();
    fade.fadeSpeed = 0.1;
}
void ParticleSystem::OnDisable()
{
    auto scene = GetScene();
    if (scene)
    {
        scene->GetRenderingScene().RemoveRenderObject(*this);
    }

    particleModifiers.clear();
}

void ParticleSystem::Serialize(Serializer* ser) const
{
    Component::Serialize(ser);
}

void ParticleSystem::Deserialize(Serializer* ser)
{
    Component::Deserialize(ser);
}

void ParticleSystem::SetParticleCount(int count)
{
    this->particleCount = count;

    if (particleWorldMatrixBuffer->GetSize() < GetParticleBufferByteSize(particleCount))
    {
        UpdatePositionBuffer();
    }
}

size_t ParticleSystem::GetParticleBufferByteSize(int particleCount)
{
    return sizeof(GPUParticle) * particleCount;
}

void ParticleSystem::Tick()
{
    IdleTick();
}

void ParticleSystem::IdleTick()
{
    float deltaTime = Time::DeltaTime();
    totalTime += deltaTime;
    ParticleModifiers::UpdateInfo updateInfo;
    updateInfo.root = this;
    updateInfo.deltaTime = deltaTime;
    updateInfo.totalTime = totalTime;

    for (auto& modifier : particleModifiers)
    {
        modifier->BeforeUpdate(deltaTime);
    }

    for (auto& modifier : particleModifiers)
    {
        for (int idx = 0; idx < particleCount; idx++)
        {
            auto& particle = particles[idx];
            modifier->Update(particle, idx, updateInfo);
        }
    }

    for (auto& modifier : particleModifiers)
    {
        modifier->AfterUpdate(deltaTime);
    }

    for (int idx = 0; idx < particleCount; idx++)
    {
        auto& particle = particles[idx];
        if (particle.IsActive())
        {
            particle.aliveFrames += 1;
        }
    }

    std::vector<GPUParticle> particleMatrices(particleCount);
    draw.particleCount = 0;
    for (int i = 0; i < particleCount; ++i)
    {
        auto& particle = particles[i];
        if (particle.IsActive())
        {
            particleMatrices[draw.particleCount].worldMatrix = glm::translate(glm::mat4(1), particle.position) *
                                                               glm::mat4_cast(particle.rotation) *
                                                               glm::scale(glm::mat4(1), particle.scale);
            particleMatrices[draw.particleCount].color = particle.color;
            draw.particleCount += 1;
        }
    }

    GetGfxDriver()->UploadBuffer(
        *particleWorldMatrixBuffer,
        (uint8_t*)particleMatrices.data(),
        GetParticleBufferByteSize(draw.particleCount),
        0
    );
}

void ParticleSystem::UpdatePositionBuffer()
{
    particleWorldMatrixBuffer =
        GetGfxDriver()->CreateBuffer(GetParticleBufferByteSize(particleCount), Gfx::BufferUsage::Storage, false, false);

    particleParameters->SetBuffer("worldMatrices", particleWorldMatrixBuffer.get());
    particles.resize(particleCount);
    for (auto& p : particles)
    {
        p.system = this;
        p.aliveFrames = -1;
    }
}

Rendering::ParticleDraw ParticleSystem::GetDraw()
{
    draw.particleShaderParameters = particleParameters->GetShaderResource();
    draw.instancingMesh = particleMesh->GetSubmesh(0);
    draw.rootTransform = gameObject->GetWorldMatrix();

    return draw;
}

void ParticleSystem::OnParticleActivate(Particle& particle)
{
    for (auto& m : particleModifiers)
    {
        m->OnParticleActivate(particle);
    }
}

void Particle::Activate()
{
    aliveFrames = 0;
    active = true;

    system->OnParticleActivate(*this);
}

namespace ParticleModifiers
{
DEFINE_OBJECT(Velocity, "E85E78E6-26E1-47DB-9357-82B17AEF7856");
DEFINE_OBJECT(Fade, "44210283-C49E-4C21-B2ED-C19E854FEDB0");
DEFINE_OBJECT(Emitter, "75F819E1-9137-405A-BD30-24D81FBC4336");

void Velocity::Update(Particle& particle, int idx, UpdateInfo& updateInfo)
{
    if (!particle.IsActive())
        return;

    particle.velocity += dir * acceleration * updateInfo.deltaTime;
    particle.velocity = glm::clamp(particle.velocity, minSpeed, maxSpeed);
    particle.position += particle.velocity * updateInfo.deltaTime;
}

void Velocity::OnParticleActivate(Particle& particle)
{
    particle.velocity = dir * initialSpeed;
}

void Emitter::BeforeUpdate(float deltaTime)
{
    timePassedSinceEmit += deltaTime;

    if (timePassedSinceEmit >= emitInterval)
    {
        emitForThisFrame = true;
        timePassedSinceEmit = 0;
        emittedCount = 0;
    }
}

void Fade::OnParticleActivate(Particle& particle)
{
    particle.color.a = 1.0f;
}

void Fade::Update(Particle& particle, int idx, UpdateInfo& updateInfo)
{
    if (particle.IsActive())
    {
        particle.color.a -= fadeSpeed * updateInfo.deltaTime;

        if (particle.color.a <= 0)
        {
            particle.Deactivate();
        }
    }
}

void Emitter::OnParticleActivate(Particle& particle)
{
    particle.position = particle.system->GetGameObject()->GetPosition();
}

void Emitter::AfterUpdate(float deltaTime)
{
    emitForThisFrame = false;
}

void Emitter::Update(Particle& particle, int idx, UpdateInfo& updateInfo)
{
    if (emitForThisFrame)
    {
        if (emittedCount < emitCountPerFrame && !particle.IsActive())
        {
            particle.Activate();
            emittedCount += 1;
        }
    }

    // if (particle.aliveFrames == 0)
    //{
    //     particle.lifetime = lifetime;
    // }
    // else
    //{
    //     particle.lifetime -= decay;
    // }

    // if (particle.lifetime < 0)
    //{
    //     if (restart)
    //     {
    //         particle.Activate();
    //     }
    //     else
    //     {
    //         particle.Deactivate();
    //     }
    // }
}
} // namespace ParticleModifiers
