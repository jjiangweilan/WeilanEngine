#include "EditorRegister.hpp"
#include "Core/AssetObject.hpp"
namespace Engine::Editor
{
    EditorRegister::EditorRegister() {}

    RefPtr<EditorRegister> EditorRegister::Instance()
    {
        if (instance == nullptr)
        {
            instance = new EditorRegister();
        }
        return instance;
    }

    UniPtr<CustomExplorer> EditorRegister::GetCustomExplorer(RefPtr<Object> object)
    {
        auto iter = customExplorers.find(object->GetTypeInfo());
        if (iter != customExplorers.end())
        {
            return iter->second(object);
        }

        return nullptr;
    }

    UniPtr<Inspector> EditorRegister::GetInspector(RefPtr<Object> object)
    {
        auto iter = inspectors.find(object->GetTypeInfo());
        if (iter != inspectors.end())
        {
            return iter->second(object);
        }

        return nullptr;
    }

    EditorRegister* EditorRegister::instance = nullptr;
}
