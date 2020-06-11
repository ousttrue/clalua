#pragma once
#include "ClangDecl.h"
#include <memory>
#include <unordered_map>
#include <filesystem>

namespace clalua
{

struct Source
{
    std::string Path;
    std::string Name() const
    {
        return std::filesystem::path(Path).stem().string();
    }

    std::vector<std::string> Imports;
    std::vector<std::shared_ptr<UserDecl>> Decls;

    void AddImport(const std::string &path)
    {
        for (auto &import : Imports)
        {
            if (import == path)
            {
                return;
            }
        }
        Imports.push_back(path);
    }

    bool AddDecl(const std::shared_ptr<UserDecl> &decl)
    {
        for (auto &d : Decls)
        {
            if (d == decl)
            {
                return false;
            }
        }
        Decls.push_back(decl);
        return true;
    }
};
using SourcePtr = std::shared_ptr<Source>;

struct ProcessorContext
{
    std::vector<std::shared_ptr<UserDecl>> DeclStack;
    std::vector<SourcePtr> SourceStack;

    ProcessorContext()
    {
    }

    void PushIfNotContains(const SourcePtr &source)
    {
        for (auto &s : SourceStack)
        {
            if (s == source)
            {
                return;
            }
        }
        SourceStack.push_back(source);
    }

    ProcessorContext Create(const std::shared_ptr<UserDecl> &decl)
    {
        ProcessorContext copy;
        copy.SourceStack = SourceStack;
        copy.DeclStack = DeclStack;
        copy.DeclStack.push_back(decl);
        return std::move(copy);
    }
};

class ClangDeclProcessor
{
    std::shared_ptr<Source> GetOrCreateSource(const std::string &path);

public:
    std::unordered_map<std::string, SourcePtr> SourceMap;
    void AddDecl(const std::shared_ptr<Decl> &decl, const ProcessorContext &context);
};

} // namespace clalua
