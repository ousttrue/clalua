#pragma once
#include <clang-c/Index.h>

class ClangCursorTraverser {
public:
  void Traverse(const CXCursor &cursor);
};
