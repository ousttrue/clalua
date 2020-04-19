#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Log.h>

#include <string>
#include <vector>

#include "ClangIndex.h"

namespace plog
{
class MyFormatter
{
  public:
    static util::nstring header() // This method returns a header for a new file. In our case it is empty.
    {
        return util::nstring();
    }

    static util::nstring format(const Record &record) // This method returns a string from a record.
    {
        util::nostringstream ss;
        ss << record.getMessage() << "\n"; // Produce a simple string with a log message.

        return ss.str();
    }
};
} // namespace plog

int main(int argc, char **argv)
{
    // setup logger
    static plog::ConsoleAppender<plog::MyFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    ClangIndex index;
    if (!index.Parse("C:/Program Files/LLVM/include/clang-c/Index.h", "C:/Program Files/LLVM/include"))
    {
        return 1;
    }

    return 0;
}
