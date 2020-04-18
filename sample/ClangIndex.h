#pragma once
#include <string>
#include <tcb/span.hpp>

class ClangIndex {
  struct ClangIndexImpl *m_impl = nullptr;

public:
  ClangIndex();
  ~ClangIndex();

  bool parse(const std::string &header, const std::string &include_dir);

  bool parse(tcb::span<std::string> headers,
             tcb::span<std::string> include_dirs,
             tcb::span<std::string> defines);
};
