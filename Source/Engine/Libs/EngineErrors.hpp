#pragma once
#include <exception>
#include <string_view>

namespace Engine
{

class EngineException : public std::exception
{
public:
    EngineException(std::string_view msg) : msg(msg){};

    const std::string& GetMessage() { return msg; }

private:
    std::string msg;
};
class NotImplemented : public EngineException
{
public:
    NotImplemented(std::string_view msg) : EngineException(msg){};
};
}; // namespace Engine
