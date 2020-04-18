#pragma once
#include <string>
#include <tcb/span.hpp>

class ClangIndex {
  struct ClangIndexImpl *m_impl = nullptr;

public:
  ClangIndex();
  ~ClangIndex();

  bool parse(tcb::span<std::string> headers, tcb::span<std::string> includes,
             tcb::span<std::string> defines, bool externC);
};
