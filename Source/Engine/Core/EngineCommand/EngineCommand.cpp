#include "EngineCommand.hpp"
#include <boost/program_options/positional_options.hpp>
#include <spdlog/spdlog.h>

class FindCommand : public IEngineCommand
{
public:
    void Execute(const std::vector<std::string>& args) override
    {
    }
};

EngineCommand::EngineCommand()
{
    RegisterCommand<FindCommand>("find");
}

void EngineCommand::Execute(const std::string& cmd)
{
    auto tokens = EngineCommand::Singleton().ParseCmd(cmd);

    if (tokens.empty())
        return;

    auto iter = commands.find(tokens[0]);
    if (iter != commands.end())
    {
        iter->second->Execute(tokens);
    }
}

EngineCommand& EngineCommand::Singleton()
{
    static EngineCommand singleton;
    return singleton;
}

std::vector<std::string> EngineCommand::ParseCmd(const std::string& cmd)
{
    std::vector<std::string> args{};

    const char* curr = &cmd[0];

    int offset = 0;
    while (curr[offset] != '\0' && offset < cmd.size())
    {
        int currentOffset = offset;
        bool skipSecondQuote = false;
        if (curr[currentOffset] == '"')
        {
            offset += 1; // skip first "
            currentOffset += 1;
            while (curr[currentOffset] != '"' && curr[currentOffset] != '\0')
                currentOffset++;

            skipSecondQuote = true;
        }
        else
        {
            while (curr[currentOffset] != ' ')
                currentOffset++;
        }

        args.push_back(std::string(curr + offset, currentOffset - offset));
        offset = currentOffset + 1;

        if (skipSecondQuote)
        {
            offset += 1;
        }
    }

    return args;
}
