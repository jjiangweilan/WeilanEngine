#include "Node.hpp"

namespace Engine::FrameGraph
{
void Node::Serialize(Serializer* s) const
{
    for (auto& c : configs)
    {
        ConfigurableType type = c.type;
        if (type == ConfigurableType::Int)
        {
            int v = std::any_cast<int>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Float)
        {
            float v = std::any_cast<float>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Format)
        {
            int v = static_cast<int>(std::any_cast<Gfx::ImageFormat>(c.data));
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec2)
        {
            glm::vec2 v = std::any_cast<glm::vec2>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec3)
        {
            glm::vec3 v = std::any_cast<glm::vec3>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec4)
        {
            glm::vec4 v = std::any_cast<glm::vec4>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec2Int)
        {
            glm::vec2 v = std::any_cast<glm::ivec2>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec3Int)
        {
            glm::vec3 v = std::any_cast<glm::ivec3>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::Vec4Int)
        {
            glm::vec4 v = std::any_cast<glm::ivec4>(c.data);
            s->Serialize(c.name, v);
        }
        else if (type == ConfigurableType::ObjectPtr)
        {
            Object* v = std::any_cast<Object*>(c.data);
            s->Serialize(c.name, v);
        }
    }
    // s->Serialize("inputProperties", inputProperties);
    // s->Serialize("outputProperties", outputProperties);
    s->Serialize("id", id);
    s->Serialize("name", name);
    s->Serialize("customName", customName);
}

void Node::Deserialize(Serializer* s)
{
    for (auto& c : configs)
    {
        ConfigurableType type = c.type;
        if (type == ConfigurableType::Int)
        {
            int v = 0;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Float)
        {
            float v = 0;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Format)
        {
            int v = 0;
            s->Deserialize(c.name, v);
            c.data = static_cast<Gfx::ImageFormat>(v);
        }
        else if (type == ConfigurableType::Vec2)
        {
            glm::vec2 v;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Vec3)
        {
            glm::vec3 v;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Vec4)
        {
            glm::vec4 v;
            s->Deserialize(c.name, v);
            c.data = v;
        }
        else if (type == ConfigurableType::Vec2Int)
        {
            glm::vec2 v;
            s->Deserialize(c.name, v);
            c.data = glm::ivec2(v);
        }
        else if (type == ConfigurableType::Vec3Int)
        {
            glm::vec3 v;
            s->Deserialize(c.name, v);
            c.data = glm::ivec3(v);
        }
        else if (type == ConfigurableType::Vec4Int)
        {
            glm::vec4 v;
            s->Deserialize(c.name, v);
            c.data = glm::ivec4(v);
        }
        else if (type == ConfigurableType::ObjectPtr)
        {
            s->Deserialize(c.name, c.dataRefHolder, [&c](void* res) { c.data = c.dataRefHolder; });
        }
    }
    // s->Serialize("inputProperties", inputProperties);
    // s->Serialize("outputProperties", outputProperties);
    s->Deserialize("id", id);

    // fix property id
    for (auto& p : inputProperties)
    {
        p.id += GetID();
    }
    for (auto& p : outputProperties)
    {
        p.id += GetID();
    }

    s->Deserialize("name", name);
    s->Deserialize("customName", customName);
}

} // namespace Engine::FrameGraph
