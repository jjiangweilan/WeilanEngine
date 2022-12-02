#pragma once

#include <string>
#include <typeinfo>
namespace Engine
{
    class Object
    {
        public:
            virtual ~Object(){};
            Object() :name() {}

            virtual void SetName(const std::string& name);
            const std::string& GetName();
            virtual const std::type_info& GetTypeInfo() = 0;

        protected:
            std::string name;
    };
}

#define DECLARE_ENGINE_OBJECT() \
    public: \
        virtual const std::type_info& GetTypeInfo() override { return typeid(*this); }
