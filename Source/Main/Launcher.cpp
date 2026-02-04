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

        if (!hasAction)
        {
            std::cout << "No action taken, maybe you should set a project path using --project";
        }
    }

    void DispatchArgs(ArgList& args, int& curr)
    {
        if (args[curr] == "--project" || args[curr] == "-p")
        {
            curr++;
            std::filesystem::path path(args[curr]);

            LaunchEngine(path.string().c_str());

            hasAction = true;
        }
    }

    void LaunchEngine(const char* projectPath)
    {
        auto engine = std::make_unique<WeilanEngine>();
        engine->Init({.projectPath = projectPath});
        engine->StartEngine();
    }

    std::unique_ptr<ArgList> argList;
    bool hasAction = false;
};

#undef main

int main(int argc, char** argv)
{
    Launcher l;
    l.ArgsParser(argc, argv);
}
