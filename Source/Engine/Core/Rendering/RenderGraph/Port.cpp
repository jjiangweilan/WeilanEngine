#include "Port.hpp"
#include "Node.hpp"

namespace Engine::RGraph
{
void Port::Connect(Port* other)
{
    if (other->connected != nullptr || connected != nullptr || connected->GetType() == other->GetType()) return;

    Port* input = type == Type::Input ? this : other;
    input->parent->AddDepth(other->parent->GetDepth() + 1);

    connected = other;
    other->connected = this;
}

void Port::Disconnect()
{
    Port* input = type == Type::Input ? this : connected;
    Port* output = type == Type::Output ? this : connected;

    input->parent->AddDepth(-output->parent->GetDepth() - 1);

    connected->connected = nullptr;
    connected == nullptr;
}
} // namespace Engine::RGraph
