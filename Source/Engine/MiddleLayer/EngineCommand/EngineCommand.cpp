#include "EngineCommand.hpp"
#include <boost/program_options/positional_options.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

EngineCommand::EngineCommand()
{}

std::string EngineCommand::GetCommandDesc(const std::string& cmd)
{
    auto tokens = EngineCommand::Singleton().ParseCmd(cmd);

    if (tokens.empty())
        return "";

    auto iter = commands.find(tokens[0]);
    if (iter != commands.end())
    {
        auto& options = iter->second->GetOptions();

        std::stringstream ss;
        options.print(ss);
        return ss.str();
    }

    return "";
}

void EngineCommand::Execute(const std::string& cmd)
{
    auto tokens = EngineCommand::Singleton().ParseCmd(cmd);

    if (tokens.empty())
        return;

    auto iter = commands.find(tokens[0]);
    if (iter != commands.end())
    {
        std::vector<const char*> charTokens{};
        for (auto& t : tokens)
        {
            charTokens.push_back(t.c_str());
        }
        iter->second->Execute(charTokens);
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
            while (curr[currentOffset] != ' ' && curr[currentOffset] != '\0')
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
