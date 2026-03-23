#include "Engine/WeilanEngine.hpp"
#include <filesystem>
#include <iostream>
#include <memory>

class Launcher
{
    using ArgList = std::vector<std::string_view>;

public:
    Launcher() {}

    void ArgsParser(int argc, char** argv)
    {
        argList = std::make_unique<ArgList>(argv + 1, argv + argc);
        for (int i = 0; i < argList->size(); ++i)
        {
            DispatchArgs(*argList, i);
        }

        if (hasAction && !projectPath.empty())
        {
            LaunchEngine(projectPath.c_str());
        }
        else if (!hasAction)
        {
            std::cout << "No action taken, maybe you should set a project path using --project";
        }
    }

    void DispatchArgs(ArgList& args, int& curr)
    {
        if (args[curr] == "--project" || args[curr] == "-p")
        {
            curr++;
            if (curr < args.size()) {
                projectPath = args[curr];
                hasAction = true;
            }
        }
        else if (args[curr] == "--enable-mcp")
        {
            enableMCP = true;
        }
    }

    void LaunchEngine(const char* projPath)
    {
        auto engine = std::make_unique<WeilanEngine>();
        engine->Init({.projectPath = projPath, .enableMCP = enableMCP});
        engine->StartEngine();
    }

    std::unique_ptr<ArgList> argList;
    bool hasAction = false;
    std::string projectPath;
    bool enableMCP = false;
};

#undef main

int main(int argc, char** argv)
{
    Launcher l;
    l.ArgsParser(argc, argv);
}
