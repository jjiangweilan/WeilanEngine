#pragma once
#include "EngineCommandBase.hpp"
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
    void Execute(const std::string& cmd);

    const CommandList& GetCommandKeys() { return commandKeys; }

    template <class CommandType>
        requires std::derived_from<CommandType, EngineCommandBase>
    bool RegisterCommand(const std::string& cmd)
    {
        ASSERT(commands.find(cmd) == commands.end() && "Command already registered");

        commandKeys.push_back(cmd);
        commands[cmd] = std::make_unique<CommandType>();

        return true;
    }

    std::vector<std::string> ParseCmd(const std::string& cmd);
    std::string GetCommandDesc(const std::string& cmd);

    static EngineCommand& Singleton();

private:
    std::unordered_map<std::string, std::unique_ptr<EngineCommandBase>> commands;
    std::vector<std::string> commandKeys;
};

#define REGISTER_ENGINE_COMMAND(CommandType, commandStr) \
    static bool _RegisterEngineCommand = EngineCommand::Singleton().RegisterCommand<CommandType>(commandStr)
