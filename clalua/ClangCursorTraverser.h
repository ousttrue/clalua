#pragma once
#include <clang-c/Index.h>
#include <unordered_map>
#include <memory>

namespace clalua
{

struct UserDecl;
std::unordered_map<uint32_t, std::shared_ptr<UserDecl>> Traverse(const CXCursor &cursor);

} // namespace clalua
