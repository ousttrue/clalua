#pragma once
#include <clang-c/Index.h>

namespace clalua
{

class ClangCursorTraverser
{
  public:
    void Traverse(const CXCursor &cursor);
};

} // namespace clalua