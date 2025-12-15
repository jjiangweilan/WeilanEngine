#pragma once
#include "Runtime/System/SceneManager/SceneManager.hpp"
#include "EditorState.hpp"
#include "EngineCommand.hpp"

namespace EngineCommands
{
class FindCommand : public EngineCommandBase
{
public:
    const boost::program_options::options_description& GetOptions() override
    {
        namespace po = boost::program_options;
        static po::options_description desc = []()
        {
            po::options_description desc("Find GameObject in Scene or any other live Object");
            desc.add_options()("name", po::value<std::string>(), "object name");

            return desc;
        }();

        return desc;
    }

    void Execute(std::span<const char*> args) override
    {
        namespace po = boost::program_options;

        auto& opts = GetOptions();

        po::positional_options_description objectNameOpt;
        objectNameOpt.add("name", 1);

        po::variables_map vm;
        po::store(po::command_line_parser(args.size(), args.data()).options(opts).positional(objectNameOpt).run(), vm);
        po::notify(vm);

        if (vm.count("name"))
        {
            std::string objName = vm["name"].as<std::string>();
            spdlog::info("Finding object with name: {}", vm["name"].as<std::string>());

            for (auto go : SceneManager::GetActiveScene()->GetAllGameObjects())
            {
                if (go && go->GetName() == objName)
                {
                    Editor::EditorState::SelectObject(go);
                }
            }
        }
    }
};

REGISTER_ENGINE_COMMAND(FindCommand, "find");
} // namespace EngineCommands
