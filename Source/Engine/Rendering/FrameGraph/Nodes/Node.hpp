#pragma once
#include "GfxDriver/GfxEnums.hpp"
#include "Libs/Serialization/Serializable.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <any>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <vector>

namespace Engine::FrameGraph
{

// frame graph ID, nodes, properties and links share the same id space
//(16 bit src unique node, 16 bit src property unique id), (16 bit node unique id), (16 bit property unique id)
// when it's a link id it uses all the 64 bit, the upper 32 bits and lower 32 bits are the target nodes and their
// note: the 32 bit all togethe makes a property id for editor
// properties respectively
using FGID = uint64_t;

// reverse 999 properties for each node, should be very enough
#define FRAME_GRAPH_PROPERTY_BIT_MASK 0xFFFFFFFF
#define FRAME_GRAPH_NODE_BIT_MASK 0xFFFF0000
#define FRAME_GRAPH_PROPERTY_BIT_COUNT 16
#define FRAME_GRAPH_NODE_PROPERTY_BIT_COUNT 32

enum class PropertyType
{
    DrawList,
    Image,
    Float // float
};

enum class ConfigurableType
{
    Int,
    Float,
    Format,
    Vec2,
    Vec3,
    Vec4,
    Vec2Int,
    Vec3Int,
    Vec4Int,
    ObjectPtr,
};

struct Configurable : public Serializable
{
    Configurable(const char* name, ConfigurableType type, std::any&& defaultVal)
        : name(name), type(type), data(defaultVal)
    {}
    // type checked constructor
    template <ConfigurableType type, class T>
    static Configurable C(const char* name, const T& val)
    {
        if constexpr (type == ConfigurableType::Int)
            static_assert(std::is_same_v<T, int>);
        else if constexpr (type == ConfigurableType::Float)
            static_assert(std::is_same_v<T, float>);
        else if constexpr (type == ConfigurableType::Format)
            static_assert(std::is_same_v<T, Gfx::ImageFormat>);
        else if constexpr (type == ConfigurableType::Vec2)
            static_assert(std::is_same_v<T, glm::vec2>);
        else if constexpr (type == ConfigurableType::Vec3)
            static_assert(std::is_same_v<T, glm::vec3>);
        else if constexpr (type == ConfigurableType::Vec4)
            static_assert(std::is_same_v<T, glm::vec4>);
        else if constexpr (type == ConfigurableType::Vec2Int)
            static_assert(std::is_same_v<T, glm::ivec2>);
        else if constexpr (type == ConfigurableType::Vec3Int)
            static_assert(std::is_same_v<T, glm::ivec3>);
        else if constexpr (type == ConfigurableType::Vec4Int)
            static_assert(std::is_same_v<T, glm::ivec4>);
        else if constexpr (type == ConfigurableType::ObjectPtr)
            static_assert(std::is_pointer_v<T>);

        return Configurable{name, type, val};
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    std::string name;
    ConfigurableType type;
    mutable std::any data;
};

class ImageProperty
{
public:
    ImageProperty(bool willCreate) : willCreate(willCreate) {}

    bool WillCreate()
    {
        return willCreate;
    }
    void* GetData();
    void SetData(void* data);

private:
    void* data;
    PropertyType type;
    bool willCreate;
};

class Node;
class Property
{
public:
    Property(Node* parent, const char* name, PropertyType type, void* data, FGID id, bool isInput)
        : parent(parent), name(name), type(type), data(data), id(id), isInput(isInput)
    {}

    PropertyType GetType() const
    {
        return type;
    }

    const std::string& GetName() const
    {
        return name;
    }

    uint32_t GetID()
    {
        return id;
    }

    Node* GetParent()
    {
        return parent;
    }

    void* GetData()
    {
        return data;
    }

    bool IsOuput()
    {
        return !isInput;
    }

    bool IsInput()
    {
        return isInput;
    }

protected:
    Node* parent;
    std::string name;
    PropertyType type;
    void* const data;
    FGID id;
    bool isInput;
};

struct Resource
{};

struct RenderNodeLink : public Resource
{
    RenderGraph::RenderNode* node;
    RenderGraph::ResourceHandle handle;
};

class BuildResources
{
public:
    template <class T>
        requires std::derived_from<T, Resource>
    T GetResource(FGID id);
};

class Node : public Object, public Serializable
{
public:
    Node(){};
    Node(const char* name, FGID id) : id(id), name(name), customName(name) {}

    FGID GetID()
    {
        return id;
    }

    virtual void Preprocess(RenderGraph::Graph& graph) = 0;
    virtual void Build(RenderGraph::Graph& graph, BuildResources& resources) = 0;

    std::span<Property> GetInput()
    {
        return inputProperties;
    }
    std::span<Property> GetOutput()
    {
        return outputProperties;
    }

    std::span<const Configurable> GetConfiurables()
    {
        return configs;
    }

    Property* GetProperty(FGID id)
    {
        for (auto& p : inputProperties)
        {
            if (p.GetID() == id)
                return &p;
        }

        for (auto& p : outputProperties)
        {
            if (p.GetID() == id)
                return &p;
        }

        return nullptr;
    }

    void SetCustomName(const char* name)
    {
        customName = name;
    }

    const std::string& GetCustomName()
    {
        return customName;
    }

    const std::string GetName() const
    {
        return name;
    }

    void Serialize(Serializer* s) const override
    {
        s->Serialize("configs", configs);
        // s->Serialize("inputProperties", inputProperties);
        // s->Serialize("outputProperties", outputProperties);
        s->Serialize("id", id);
        s->Serialize("name", name);
        s->Serialize("customName", customName);
    }

    void Deserialize(Serializer* s) override
    {
        s->Serialize("configs", configs);
        // s->Serialize("inputProperties", inputProperties);
        // s->Serialize("outputProperties", outputProperties);
        s->Serialize("id", id);
        s->Serialize("name", name);
        s->Serialize("customName", customName);
    }

protected:
    std::vector<Configurable> configs;

    template <class T>
    T GetConfigurableVal(const char* name)
    {
        for (auto& c : configs)
        {
            if (strcmp(c.name.c_str(), name) == 0)
            {
                return std::any_cast<c.data>;
            }
        }

        return T{};
    }

    FGID AddInputProperty(const char* name, PropertyType type, void* data)
    {
        FGID id = GetID() + (inputProperties.size() + outputProperties.size() + 1);
        inputProperties.emplace_back(this, name, type, data, id,
                                     true); // plus one to avoid the same id as node itself

        propertyIDs[name] = id;

        return id;
    }

    FGID AddOutputProperty(const char* name, PropertyType type, void* data)
    {
        FGID id = GetID() + (inputProperties.size() + outputProperties.size() + 1);
        outputProperties.emplace_back(
            this,
            name,
            type,
            data,
            id,
            false
        ); // plus one to avoid the same id as node itself

        propertyIDs[name] = id;

        return id;
    }

    std::unordered_map<std::string, FGID> propertyIDs;

private:
    std::vector<Property> inputProperties;
    std::vector<Property> outputProperties;
    FGID id;
    std::string name;
    std::string customName;
};
} // namespace Engine::FrameGraph
