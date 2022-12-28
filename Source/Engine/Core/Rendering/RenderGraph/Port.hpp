#pragma once
#include "Code/UUID.hpp"
#include <typeinfo>

namespace Engine::RGraph
{
class Node;
/**
 * a port can either input or output a ResourceRef, this is a wrapper around
 * that resource, a resource reference can exist before the actual resource is created
 */
class ResourceRef
{
public:
    ResourceRef(const type_info& typeInfo) : typeInfo(&typeInfo), val(nullptr) {}
    ResourceRef(const ResourceRef& other) : typeInfo(other.typeInfo), val(other.val) {}
    ResourceRef& operator=(const ResourceRef& other)
    {
        this->typeInfo = other.typeInfo;
        this->val = other.val;
        this->uuid = other.uuid;

        return *this;
    }

    /**
     * assign a value to a resourceRef
     */
    template <class T>
    void SetVal(T* val)
    {
        assert(typeid(T) == *typeInfo);
        this->val = val;
    }
    bool IsType(const std::type_info& type)
    {
        if (typeInfo) return type == *typeInfo;
        return false;
    };
    bool IsNull() { return val == nullptr; }
    void* GetVal() { return val; }

private:
    const std::type_info* typeInfo;
    void* val;
    UUID uuid;

    friend struct ResourceRefHash;
};

struct ResourceRefHash
{
    size_t operator()(const ResourceRef& ref) { return std::hash<UUID>()(ref.uuid); }
};

class Port
{
public:
    enum class Type
    {
        Input,
        Output
    };
    enum class ConnectionType
    {
        Connect,
        Disconnect
    };

    Port(Node* parent, const char* name, Type type, const std::type_info& dataType)
        : parent(parent), type(type), dataType(dataType), resourceRef(dataType), name(name)
    {}
    virtual ~Port() {}

    Type GetType() { return type; }

    /**
     * Get the value that is passed through the port, template is used for type
     * check
     *
     */
    ResourceRef& GetResource() { return resourceRef; }

    bool CanConnect(Port* port) { return dataType == port->dataType; }

    Node* GetNode() { return parent; }
    Port* GetConnectedPort() { return connected; }
    void Connect(Port* other);
    void Disconnect();

protected:
    Node* parent;
    Port* connected;
    Type type;
    const std::type_info& dataType;
    ResourceRef resourceRef;
    std::string name;

    friend class NodeGraph;
};
} // namespace Engine::RGraph
