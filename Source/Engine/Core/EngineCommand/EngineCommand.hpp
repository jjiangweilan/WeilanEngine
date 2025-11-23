#pragma once
#include "IEngineCommand.hpp"
#include "Libs/Assert.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class EngineCommand
{
public:
    using CommandList = std::vector<std::string>;

    EngineCommand();
    void Execute(const char* cmd);

    const CommandList& GetCommandKeys() { return commandKeys; }

    template <class CommandType>
        requires std::derived_from<CommandType, IEngineCommand>
    void RegisterCommand(const std::string& cmd)
    {
        ASSERT(commands.find(cmd) == commands.end() && "Command already registered");

        commandKeys.push_back(cmd);
        commands[cmd] = std::make_unique<CommandType>();
    }

    static EngineCommand& Singleton();

private:
    std::unordered_map<std::string, std::unique_ptr<IEngineCommand>> commands;
    std::vector<std::string> commandKeys;
};
