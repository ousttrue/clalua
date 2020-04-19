#include "ClangCursorTraverser.h"

#include <clang-c/Index.h>
#include <plog/Log.h>

#include <functional>

#include "enum_name.h"

using CallbackFunc = std::function<void(const CXCursor &)>;

static CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, void *data)
{
    (*reinterpret_cast<CallbackFunc *>(data))(cursor);
    return CXChildVisit_Continue;
};
static void processChildren(const CXCursor &cursor, const CallbackFunc &callback)
{
    clang_visitChildren(cursor, &visitor, (void *)&callback);
}

class ScopedCXString
{
    const CXString &m_str;
    const char *m_c_str;

    ScopedCXString(const ScopedCXString &) = delete;
    ScopedCXString &operator=(const ScopedCXString &) = delete;

  public:
    ScopedCXString(const CXString &str) : m_str(str)
    {
        m_c_str = clang_getCString(str);
    }
    ~ScopedCXString()
    {
        clang_disposeString(m_str);
    }
    const char *c_str() const
    {
        return m_c_str;
    }
    std::string_view str_view() const
    {
        return m_c_str;
    }
};

class ScopedCXTokens
{
    CXTranslationUnit m_tu;
    CXToken *m_tokens;
    uint32_t m_count;

    ScopedCXTokens(const ScopedCXTokens &) = delete;
    ScopedCXTokens &operator=(const ScopedCXTokens &) = delete;

  public:
    ScopedCXTokens(const CXCursor &cursor)
    {
        auto range = clang_getCursorExtent(cursor);
        m_tu = clang_Cursor_getTranslationUnit(cursor);
        clang_tokenize(m_tu, range, &m_tokens, &m_count);
    }
    ~ScopedCXTokens()
    {
        clang_disposeTokens(m_tu, m_tokens, m_count);
    }
    uint32_t size() const
    {
        return m_count;
    }

    ScopedCXString Spelling(uint32_t index) const
    {
        return ScopedCXString(clang_getTokenSpelling(m_tu, m_tokens[index]));
    }
};

class TraverserImpl
{
  public:
    void TraverseChildren(const CXCursor &cursor)
    {
        processChildren(cursor, std::bind(&TraverserImpl::Traverse, this, std::placeholders::_1));
    }

  private:
    void Traverse(const CXCursor &cursor)
    {
        ScopedCXString spelling(clang_getCursorSpelling(cursor));

        auto location = clang_getCursorLocation(cursor);
        CXFile file = nullptr;
        uint32_t line;
        uint32_t column;
        uint32_t offset;
        if (!clang_equalLocations(location, clang_getNullLocation()))
        {
            clang_getInstantiationLocation(location, &file, &line, &column, &offset);
        }

        if (file)
        {
            ScopedCXString fileStr(clang_getFileName(file));
            LOGD << fileStr.c_str() << ":" << line << ":" << column << ' ' // << ":" << offset
                 << enum_name_map<CXCursorKind, (int)CXCursor_OverloadCandidate>::get(cursor.kind) << '('
                 << (int)cursor.kind << ')' << " \"" << spelling.c_str() << "\" ";
        }
        else
        {
            LOGD << enum_name_map<CXCursorKind, (int)CXCursor_OverloadCandidate>::get(cursor.kind) << '('
                 << (int)cursor.kind << ')' << " \"" << spelling.c_str() << "\" ";
        }

        switch (cursor.kind)
        {
        case CXCursor_InclusionDirective:
        case CXCursor_ClassTemplate:
        case CXCursor_ClassTemplatePartialSpecialization:
        case CXCursor_FunctionTemplate:
        case CXCursor_UsingDeclaration:
        case CXCursor_StaticAssert:
            // skip
            break;

        case CXCursor_MacroDefinition:
            // parseMacroDefinition(cursor);
            break;

        case CXCursor_MacroExpansion:
            if (spelling.str_view() == "DEFINE_GUID")
            {
                //   auto tokens = getTokens(cursor);
                //   scope(exit)
                //       clang_disposeTokens(tu, tokens.ptr, cast(uint)
                //       tokens.length);
                //   string[] tokenSpellings =
                //       tokens.map !(t = > tokenToString(cursor,
                //       t)).array();
                //   if (tokens.length == 26) {
                //     auto name = tokenSpellings[2];
                //     if (name.startsWith("IID_")) {
                //       name = name[4.. $];
                //     }
                //     m_uuidMap[name] = tokensToUUID(tokenSpellings[4..
                //     $]);
                //   } else {
                //     debug auto a = 0;
                //   }
            }
            break;

        case CXCursor_Namespace:
            //   {
            //     context = context.enterNamespace(spelling);
            //     foreach (child; cursor.getChildren()) {
            //       traverse(child, context.getChild());
            //     }
            //   }
            break;

        case CXCursor_UnexposedDecl: {
            ScopedCXTokens tokens(cursor);

            if (tokens.size() >= 2)
            {
                // extern C
                if (tokens.Spelling(0).str_view() == "extern" && tokens.Spelling(1).str_view() == "\"C\"")
                {
                    // context.isExternC = true;
                }
            }

            TraverseChildren(cursor);
        }
        break;

        case CXCursor_TypedefDecl:
            // parseTypedef(cursor);
            break;

        case CXCursor_FunctionDecl:
            //   {
            //     auto decl =
            //         parseFunction(cursor, &context,
            //         clang_getCursorResultType(cursor));
            //     if (decl) {
            //       auto header = getOrCreateHeader(cursor);
            //       header.types ~ = decl;
            //     }
            //   }
            break;

        case CXCursor_StructDecl:
        case CXCursor_ClassDecl:
            // parseStruct(cursor, context.enterNamespace(spelling), false);
            break;

        case CXCursor_UnionDecl:
            // parseStruct(cursor, context.enterNamespace(spelling), true);
            break;

        case CXCursor_EnumDecl:
            // parseEnum(cursor);
            break;

        case CXCursor_VarDecl:
            break;

        default:
            throw std::runtime_error("unknown CXCursorKind");
        }
    }
};

void ClangCursorTraverser::Traverse(const CXCursor &cursor)
{
    TraverserImpl impl;
    impl.TraverseChildren(cursor);
}
