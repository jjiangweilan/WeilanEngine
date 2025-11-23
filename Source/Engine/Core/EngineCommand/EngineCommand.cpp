#include "EngineCommand.hpp"

class SudoCommand : public IEngineCommand
{
};

EngineCommand::EngineCommand()
{
    RegisterCommand<SudoCommand>("Find GameObject");
    RegisterCommand<SudoCommand>("Hello");
}

void EngineCommand::Execute(const char* cmd)
{
}

EngineCommand& EngineCommand::Singleton()
{
    static EngineCommand singleton;
    return singleton;
}
