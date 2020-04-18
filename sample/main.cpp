#include "ClangIndex.h"
#include <plog/Log.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  // setup logger
  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::verbose, &consoleAppender);

  std::string headers[] = {
      "C:/Program Files/LLVM/include/clang-c/Index.h",
  };
  std::string includes[] = {
      "C:/Program Files/LLVM/include",
  };
  std::vector<std::string> defines;

  ClangIndex index;
  if (!index.parse(headers, includes, defines, true)) {
    return 1;
  }

  return 0;
}
