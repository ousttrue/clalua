#include "ClangIndex.h"
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Log.h>
#include <string>
#include <vector>


int main(int argc, char **argv) {
  // setup logger
  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::verbose, &consoleAppender);

  ClangIndex index;
  if (!index.parse("C:/Program Files/LLVM/include/clang-c/Index.h",
                   "C:/Program Files/LLVM/include")) {
    return 1;
  }

  return 0;
}
