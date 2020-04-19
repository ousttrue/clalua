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
static void processChildren(const CXCursor &cursor,
                            const CallbackFunc &callback) {
  clang_visitChildren(cursor, &visitor, (void *)&callback);
}

class ScopedCXString {
  const CXString &m_str;
  const char *m_c_str;

  ScopedCXString(const ScopedCXString &) = delete;
  ScopedCXString &operator=(const ScopedCXString &) = delete;

public:
  ScopedCXString(const CXString &str) : m_str(str) {
    m_c_str = clang_getCString(str);
  }
  ~ScopedCXString() { clang_disposeString(m_str); }
  const char *c_str() const { return m_c_str; }
};

class TraverserImpl {
public:
  void Traverse(const CXCursor &cursor) {

    ScopedCXString cursorSpelling(clang_getCursorSpelling(cursor));

    auto location = clang_getCursorLocation(cursor);
    if (clang_equalLocations(location, clang_getNullLocation())) {
      // no location
      LOGD << " \"" << cursorSpelling.c_str() << "\" " << (int)cursor.kind
           << " => "
           << enum_name_map<CXCursorKind, (int)CXCursor_OverloadCandidate>::get(
                  cursor.kind);

    } else {

      CXFile file;
      uint32_t line;
      uint32_t column;
      uint32_t offset;
      clang_getInstantiationLocation(location, &file, &line, &column, &offset);
      if (!file) {
        // no file
        LOGD << "no file";
      } else {
        ScopedCXString fileStr(clang_getFileName(file));

        LOGD
            << fileStr.c_str() << ":" << line << ":"
            << column // << ":" << offset
            << " \"" << cursorSpelling.c_str() << "\" " << (int)cursor.kind
            << " => "
            << enum_name_map<CXCursorKind,
                             (int)CXCursor_OverloadCandidate>::get(cursor.kind);
      }
    }
  }
};

void ClangCursorTraverser::Traverse(const CXCursor &cursor) {

  TraverserImpl impl;

  CallbackFunc callback = [&impl](const CXCursor &child) {
    impl.Traverse(child);
  };

  processChildren(cursor, callback);
}
