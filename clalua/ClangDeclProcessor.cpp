#include "ClangDeclProcessor.h"

namespace clalua
{

std::shared_ptr<Source> ClangDeclProcessor::GetOrCreateSource(const std::string &path)
{
    auto found = SourceMap.find(path);
    if (found != SourceMap.end())
    {
        return found->second;
    }

    auto source = std::make_shared<Source>();
    source->Path = path;
    SourceMap.insert(std::make_pair(path, source));

    return source;
}

void ClangDeclProcessor::AddDecl(const std::shared_ptr<Decl> &decl, const ProcessorContext &_context)
{
    auto context = _context;

    // recursion check
    for (auto &d : context.DeclStack)
    {
        if (d == decl)
        {
            // stop
            return;
        }
    }

    auto userDecl = std::dynamic_pointer_cast<UserDecl>(decl);
    if (!userDecl)
    {
        return;
    }

    // get source
    auto source = GetOrCreateSource(userDecl->path);
    for (auto &s : context.SourceStack)
    {
        s->AddImport(source->Path);
    }
    context.PushIfNotContains(source);

    // add decl
    source->AddDecl(userDecl);

    // recursive AddDecl
    {
        auto functionDecl = std::dynamic_pointer_cast<FunctionDecl>(userDecl);
        if (functionDecl)
        {
            AddDecl(functionDecl->returnType.decl, context.Create(userDecl));
            for (auto &param : functionDecl->params)
            {
                AddDecl(param.ref.decl, context.Create(userDecl));
            }
            return;
        }
    }
    {
        auto typedefDecl = std::dynamic_pointer_cast<Typedef>(userDecl);
        if (typedefDecl)
        {
            AddDecl(typedefDecl->ref.decl, context.Create(userDecl));
            return;
        }
    }

    {
        auto structDecl = std::dynamic_pointer_cast<StructDecl>(userDecl);
        if (structDecl)
        {
            // if (structDecl->iid.empty)
            // {
            //     if (structDecl.name in m_parser.m_uuidMap)
            //     {
            //         structDecl.iid = m_parser.m_uuidMap[structDecl.name];
            //     }
            // }

            // if (structDecl->base)
            // {
            //     addDecl(_decl ~structDecl.base, from);
            // }

            for (auto &field : structDecl->fields)
            {
                AddDecl(field.ref.decl, context.Create(userDecl));
            }

            // for (auto &method : structDecl->methods)
            // {
            //     addDecl(_decl ~method.ret.type, from);
            //     foreach (param; method.params)
            //     {
            //         addDecl(_decl ~param.typeref.type, from);
            //     }
            // }

            return;
        }
    }
}

} // namespace clalua
