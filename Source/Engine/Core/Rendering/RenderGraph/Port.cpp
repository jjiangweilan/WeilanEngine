#include "Port.hpp"
#include "Node.hpp"

namespace Engine::RGraph
{
void Port::Connect(Port* other)
{
    auto iter = std::find(connected.begin(), connected.end(), other);
    if (iter != connected.end()) return;
    if ((!isMultiPort && !other->connected.empty()) || GetType() == other->GetType()) return;

    Port* input = type == Type::Input ? this : other;
    Port* output = type == Type::Output ? this : other;
    input->SetResource(output->GetResource());
    input->parent->AddDepth(other->parent->GetDepth() + 1);

    connected.push_back(other);
    other->connected.push_back(this);

    if (connectionCallback) connectionCallback(parent, other, ConnectionType::Connect);
    if (other->connectionCallback) other->connectionCallback(other->parent, this, ConnectionType::Connect);
}

void Port::Disconnect(Port* other)
{
    auto iter = std::find(connected.begin(), connected.end(), other);
    if (iter == connected.end()) return;

    Port* input = type == Type::Input ? this : other;
    Port* output = type == Type::Output ? this : other;

    input->RemoveResource(input->GetResource());
    input->parent->AddDepth(-output->parent->GetDepth() - 1);

    if (connectionCallback) connectionCallback(parent, other, ConnectionType::Disconnect);
    if (other->connectionCallback) other->connectionCallback(other->parent, this, ConnectionType::Disconnect);

    connected.erase(iter);
    auto iterOther = std::find(connected.begin(), connected.end(), this);
    other->connected.erase(iterOther);
}
} // namespace Engine::RGraph
