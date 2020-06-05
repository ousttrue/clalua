#pragma once
#include <tcb/span.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace clalua
{
class UserDecl;

std::unordered_map<uint32_t, std::shared_ptr<UserDecl>> Parse(const std::string &header, const std::string &include_dir);

std::unordered_map<uint32_t, std::shared_ptr<UserDecl>> Parse(tcb::span<std::string> headers, tcb::span<std::string> include_dirs, tcb::span<std::string> defines);

} // namespace clalua