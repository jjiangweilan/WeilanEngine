
#include <boost/program_options.hpp>
#include <span>

class EngineCommandBase
{
public:
    virtual void Execute(std::span<const char*> args) = 0;
    virtual const boost::program_options::options_description& GetOptions() = 0;

protected:
};
