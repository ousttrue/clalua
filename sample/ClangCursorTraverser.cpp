#include "ClangCursorTraverser.h"
#include "enum_name.h"
#include <clang-c/Index.h>
#include <functional>
#include <plog/Log.h>


using CallbackFunc = std::function<void(const CXCursor &)>;

static CXChildVisitResult visitor(CXCursor cursor, CXCursor parent,
                                  void *data) {
  (*reinterpret_cast<CallbackFunc *>(data))(cursor);
  return CXChildVisit_Continue;
};
static void ProcessChildren(const CXCursor &cursor,
                            const CallbackFunc &callback) {
  clang_visitChildren(cursor, &visitor, (void *)&callback);
}

void ClangCursorTraverser::Traverse(const CXCursor &cursor) {

  CallbackFunc callback = [](const CXCursor &child) {
    LOGD << (int)child.kind << " => "
         << enum_name_map<CXCursorKind, (int)CXCursor_OverloadCandidate>::get(
                child.kind);
  };

  ProcessChildren(cursor, callback);
}
