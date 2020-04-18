#include "ClangIndex.h"
#include "ClangCursorTraverser.h"
#include <clang-c/Index.h>
#include <fmt/format.h>
#include <functional>
#include <tcb/span.hpp>

struct ClangIndexImpl {
  CXIndex m_index = nullptr;
  CXTranslationUnitImpl *m_tu = nullptr;

  ClangIndexImpl() : m_index(clang_createIndex(0, 1)) {}
  ~ClangIndexImpl() {
    if (m_tu) {
      clang_disposeTranslationUnit(m_tu);
      m_tu = nullptr;
    }
    clang_disposeIndex(m_index);
  }

  bool Parse(tcb::span<std::string> headers, tcb::span<std::string> includes,
             tcb::span<std::string> defines) {
    std::vector<std::string> params = {
        "-x",
        "c++",
        "-target",
        "x86_64-windows-msvc",
        "-fms-compatibility-version=18",
        "-fdeclspec",
        "-fms-compatibility",
    };
    for (auto &include : includes) {
      params.push_back(fmt::format("-I{0}", include));
    }
    for (auto &define : defines) {
      params.push_back(fmt::format("-D{0}", define));
    }

    // LOGD << params;
    if (!getTU(headers, params)) {
      return false;
    }

    return true;
  }

  CXCursor GetRootCursor() { return clang_getTranslationUnitCursor(m_tu); }

private:
  bool getTU(tcb::span<std::string> headers, tcb::span<std::string> params) {
    std::vector<const char *> c_params;
    for (auto &param : params) {
      c_params.push_back(param.c_str());
    }

    auto options = CXTranslationUnit_DetailedPreprocessingRecord |
                   CXTranslationUnit_SkipFunctionBodies;
    if (headers.size() == 1) {
      m_tu = clang_parseTranslationUnit(m_index, headers[0].c_str(),
                                        c_params.data(), params.size(), nullptr,
                                        0, options);
    } else {
      std::string sb;
      for (auto &header : headers) {
        sb += fmt::format("#include \"{0}\"\n", header);
      }

      // use unsaved files
      std::vector<CXUnsavedFile> files;
      files.push_back(CXUnsavedFile{"__tmp__dclangen__.h", sb.c_str(),
                                    static_cast<unsigned long>(sb.size())});

      m_tu = clang_parseTranslationUnit(m_index, "__tmp__dclangen__.h",
                                        c_params.data(), params.size(),
                                        files.data(), files.size(), options);
    }
    return m_tu != nullptr;
  }
};

ClangIndex::ClangIndex() : m_impl(new ClangIndexImpl) {}

ClangIndex::~ClangIndex() { delete m_impl; }

bool ClangIndex::Parse(const std::string &header,
                       const std::string &include_dir) {

  std::string headers[] = {
      header,
  };
  std::string include_dirs[] = {
      include_dir,
  };
  std::vector<std::string> defines;

  return Parse(headers, include_dirs, defines);
}

bool ClangIndex::Parse(tcb::span<std::string> headers,
                       tcb::span<std::string> includes,
                       tcb::span<std::string> defines) {
  if (!m_impl->Parse(headers, includes, defines)) {
    return false;
  }

  auto cursor = m_impl->GetRootCursor();

  ClangCursorTraverser traverser;
  traverser.Traverse(cursor);

  return true;
}
