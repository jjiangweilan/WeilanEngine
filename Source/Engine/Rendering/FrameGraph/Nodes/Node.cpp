#include "Node.hpp"

namespace Engine::FrameGraph
{
void Configurable::Serialize(Serializer* s) const
{
    s->Serialize("name", name);
    s->Serialize("type", static_cast<int>(type));

    if (type == ConfigurableType::Int)
    {
        int v = std::any_cast<int>(data);
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Float)
    {
        float v = std::any_cast<float>(data);
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Format)
    {
        int v = static_cast<int>(std::any_cast<Gfx::ImageFormat>(data));
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Vec2)
    {
        glm::vec2 v = std::any_cast<glm::vec2>(data);
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Vec3)
    {
        glm::vec3 v = std::any_cast<glm::vec3>(data);
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Vec4)
    {
        glm::vec4 v = std::any_cast<glm::vec4>(data);
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Vec4Int)
    {
        glm::vec4 v = std::any_cast<glm::ivec4>(data);
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Vec3Int)
    {
        glm::vec3 v = std::any_cast<glm::ivec3>(data);
        s->Serialize("data", v);
    }
    else if (type == ConfigurableType::Vec2Int)
    {
        glm::vec2 v = std::any_cast<glm::ivec2>(data);
        s->Serialize("data", v);
    }
}

void Configurable::Deserialize(Serializer* s)
{
    s->Deserialize("name", name);
    int t = 0;
    s->Deserialize("type", t);
    type = static_cast<ConfigurableType>(t);
    if (type == ConfigurableType::Int)
    {
        int v = 0;
        s->Deserialize("data", v);
        data = v;
    }
    else if (type == ConfigurableType::Float)
    {
        float v = 0;
        s->Deserialize("data", v);
        data = v;
    }
    else if (type == ConfigurableType::Format)
    {
        int v = 0;
        s->Deserialize("data", v);
        data = static_cast<Gfx::ImageFormat>(v);
    }
    else if (type == ConfigurableType::Vec2)
    {
        glm::vec2 v;
        s->Deserialize("data", v);
        data = v;
    }
    else if (type == ConfigurableType::Vec3)
    {
        glm::vec3 v;
        s->Deserialize("data", v);
        data = v;
    }
    else if (type == ConfigurableType::Vec4)
    {
        glm::vec4 v;
        s->Deserialize("data", v);
        data = v;
    }
    else if (type == ConfigurableType::Vec4Int)
    {
        glm::vec4 v;
        s->Deserialize("data", v);
        data = glm::ivec4(v);
    }
    else if (type == ConfigurableType::Vec3Int)
    {
        glm::vec3 v;
        s->Deserialize("data", v);
        data = glm::ivec3(v);
    }
    else if (type == ConfigurableType::Vec2Int)
    {
        glm::vec2 v;
        s->Deserialize("data", v);
        data = glm::ivec2(v);
    }
}
} // namespace Engine::FrameGraph
