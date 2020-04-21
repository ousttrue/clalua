#pragma once
#include <string>
#include <tcb/span.hpp>

namespace clalua
{
  
class ClangIndex
{
    struct ClangIndexImpl *m_impl = nullptr;

  public:
    ClangIndex();
    ~ClangIndex();

    bool Parse(const std::string &header, const std::string &include_dir);

    bool Parse(tcb::span<std::string> headers, tcb::span<std::string> include_dirs, tcb::span<std::string> defines);
};

} // namespace clalua