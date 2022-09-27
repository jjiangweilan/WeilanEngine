#pragma once

#include "Code/Ptr.hpp"
#include "Extension/CustomExplorer.hpp"
#include "Extension/Inspector.hpp"
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
namespace Engine
{
    class AssetObject;
}
namespace Engine::Editor
{
    // many asset objects need to define their custom behaviours in editor
    // this is the place to register these behaviours
    class EditorRegister
    {
        public:
            template<class Target, class Explorer>
            bool RegisterCustomExplorer();

            template<class Target, class Inspector>
            bool RegisterInspector();

            UniPtr<CustomExplorer> GetCustomExplorer(RefPtr<Object> object);
            UniPtr<Inspector> GetInspector(RefPtr<Object> object);

        private:
            EditorRegister();

            std::unordered_map<std::type_index, std::function<UniPtr<CustomExplorer>(RefPtr<Object>)>> customExplorers;
            std::unordered_map<std::type_index, std::function<UniPtr<Inspector>(RefPtr<Object>)>> inspectors;
            // std::unordered_map<size_t, InspectorGenerator> inspectorGenerators;


        public:
            static RefPtr<EditorRegister> Instance();

        private:
            static EditorRegister* instance;
    };


    template<class Target, class Explorer>
    bool EditorRegister::RegisterCustomExplorer()
    {
        auto& typeInfo = typeid(Target);
        auto iter = customExplorers.find(typeInfo);

        if (iter == customExplorers.end())
        {
            customExplorers[typeInfo] = [](RefPtr<Object> object) {
                return MakeUnique<Explorer>(object);
            };
            return true;
        }

        SPDLOG_WARN("Duplicate editor register are ignored");
        return false;
    }

    template<class Target, class Inspector>
    bool EditorRegister::RegisterInspector()
    {
        auto& typeInfo = typeid(Target);
        auto iter = inspectors.find(typeInfo);

        if (iter == inspectors.end())
        {
            inspectors[typeInfo] = [](RefPtr<Object> object) {
                return MakeUnique<Inspector>(object);
            };
            return true;
        }

        SPDLOG_WARN("Duplicate editor register are ignored");
        return false;
    }
}
