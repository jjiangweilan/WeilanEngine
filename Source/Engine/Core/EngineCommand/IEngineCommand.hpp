
#include <string>
#include <vector>
class IEngineCommand
{
public:
    virtual void Execute(const std::vector<std::string>& args) = 0;

protected:
};
