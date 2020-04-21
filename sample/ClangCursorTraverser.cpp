#include "ClangCursorTraverser.h"
#include "ClangDecl.h"
#include "enum_name.h"
#include <clang-c/Index.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <plog/Log.h>
#include <tcb/span.hpp>

namespace clalua
{

using CallbackFunc = std::function<CXChildVisitResult(const CXCursor &)>;

static std::vector<uint8_t> readAllBytes(const std::filesystem::path &path)
{
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    auto pos = ifs.tellg();
    std::vector<uint8_t> result(pos);
    ifs.seekg(0, std::ios::beg);
    ifs.read((char *)result.data(), pos);
    return result;
}

static CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, void *data)
{
    return (*reinterpret_cast<CallbackFunc *>(data))(cursor);
};
static void processChildren(const CXCursor &cursor, const CallbackFunc &callback)
{
    clang_visitChildren(cursor, &visitor, (void *)&callback);
}

// https://joshpeterson.github.io/identifying-a-forward-declaration-with-libclang
static bool isForwardDeclaration(const CXCursor &cursor)
{
    auto definition = clang_getCursorDefinition(cursor);

    // If the definition is null, then there is no definition in this translation
    // unit, so this cursor must be a forward declaration.
    if (clang_equalCursors(definition, clang_getNullCursor()))
        return true;

    // If there is a definition, then the forward declaration and the definition
    // are in the same translation unit. This cursor is the forward declaration if
    // it is _not_ the definition.
    return !clang_equalCursors(cursor, definition);
}

// http://clang-developers.42468.n3.nabble.com/llibclang-CXTypeKind-char-types-td3754411.html
std::shared_ptr<Primitive> getPrimitive(CXTypeKind kind)
{
    switch (kind)
    {
    case CXType_Void:
        return Void::instance();
    case CXType_Bool:
        return std::make_shared<Bool>();
        // Int
    case CXType_Char_S:
    case CXType_SChar:
        return std::make_shared<Int8>();
    case CXType_Short:
        return std::make_shared<Int16>();
    case CXType_Int:
    case CXType_Long:
        return std::make_shared<Int32>();
    case CXType_LongLong:
        return std::make_shared<Int64>();
        // UInt
    case CXType_Char_U:
    case CXType_UChar:
        return std::make_shared<UInt8>();
    case CXType_UShort:
    case CXType_WChar:
        return std::make_shared<UInt16>();
    case CXType_UInt:
    case CXType_ULong:
        return std::make_shared<UInt32>();
    case CXType_ULongLong:
        return std::make_shared<UInt64>();
        // Float
    case CXType_Float:
        return std::make_shared<Float>();
    case CXType_Double:
        return std::make_shared<Double>();
    case CXType_LongDouble:
        return std::make_shared<LongDouble>();

    default:
        return nullptr;
    }
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
    std::string str() const
    {
        return m_c_str;
    }
};

struct Location
{
    CXFile file = nullptr;
    uint32_t line = 0;
    uint32_t column = 0;
    uint32_t offset = 0;

    uint32_t begin = 0;
    uint32_t end = 0;

    static Location get(const CXCursor &cursor)
    {
        auto location = clang_getCursorLocation(cursor);
        Location l{};
        if (!clang_equalLocations(location, clang_getNullLocation()))
        {
            clang_getInstantiationLocation(location, &l.file, &l.line, &l.column, &l.offset);
            auto extent = clang_getCursorExtent(cursor);
            auto begin = clang_getRangeStart(extent);
            clang_getInstantiationLocation(begin, &l.file, &l.line, &l.column, &l.begin);
            auto end = clang_getRangeEnd(extent);
            clang_getInstantiationLocation(end, nullptr, nullptr, nullptr, &l.end);
        }

        return l;
    }

    std::string path() const
    {
        ScopedCXString fileStr(clang_getFileName(file));
        return std::string(fileStr.str_view());
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

    ScopedCXString spelling(uint32_t index) const
    {
        return ScopedCXString(clang_getTokenSpelling(m_tu, m_tokens[index]));
    }
};

struct Context
{
    const Context *parent = nullptr;
    bool isExternC = false;
    std::string_view namespaceName;

    Context createChild() const
    {
        return {
            .parent = this,
            .isExternC = isExternC,
        };
    }

    Context enterNamespace(const std::string_view &view) const
    {
        return {
            .parent = this,
            .isExternC = isExternC,
            .namespaceName = view,
        };
    }
};

struct Source
{
    std::string path;
    std::vector<uint8_t> data;
};

auto D3D11_KEY = "MIDL_INTERFACE(\"";
auto D2D1_KEY = "DX_DECLARE_INTERFACE(\"";
auto DWRITE_KEY = "DWRITE_DECLARE_INTERFACE(\"";

// UUID getUUID(string src)
// {
//     if (src.startsWith(D3D11_KEY))
//     {
//         return parseUUID(src[D3D11_KEY.length .. $ - 2]);
//     }
//     else if (src.startsWith(D2D1_KEY))
//     {
//         return parseUUID(src[D2D1_KEY.length .. $ - 2]);
//     }
//     else if (src.startsWith(DWRITE_KEY))
//     {
//         return parseUUID(src[DWRITE_KEY.length .. $ - 2]);
//     }
//     else
//     {
//         return UUID();
//     }
// }

class TraverserImpl
{
  public:
    void TraverseChildren(const CXCursor &cursor, const Context &context)
    {
        processChildren(cursor, std::bind(&TraverserImpl::traverse, this, std::placeholders::_1, context));
    }

  private:
    std::unordered_map<std::string, std::shared_ptr<Source>> m_sourceMap;

    std::shared_ptr<Source> getOrCreateSource(const CXCursor &cursor)
    {
        auto location = Location::get(cursor);
        auto path = location.path();
        auto found = m_sourceMap.find(path);
        if (found != m_sourceMap.end())
        {
            return found->second;
        }

        auto source = std::make_shared<Source>();
        source->path = path;
        source->data = readAllBytes(path);
        return source;
    }

    tcb::span<uint8_t> getSource(const CXCursor &cursor)
    {
        auto source = getOrCreateSource(cursor);
        if (!source || source->data.empty())
        {
            return {};
        }
        auto location = Location::get(cursor);
        auto p = source->data.data();
        return tcb::span<uint8_t>(p + location.begin, p + location.end);
    }

    std::unordered_map<uint32_t, std::shared_ptr<UserDecl>> m_declMap;
    void pushDecl(const CXCursor &cursor, const std::shared_ptr<UserDecl> &decl)
    {
        m_declMap.insert(std::make_pair(decl->hash, decl));
    }

    template <typename T> std::shared_ptr<T> getDecl(const CXCursor &cursor)
    {
        auto hash = clang_hashCursor(cursor);
        auto found = m_declMap.find(hash);
        if (found == m_declMap.end())
        {
            // not found
            return nullptr;
        }
        return std::dynamic_pointer_cast<T>(found->second);
    }

    std::shared_ptr<Decl> typeToDecl(const CXCursor &cursor)
    {
        auto cursorType = clang_getCursorType(cursor);
        return typeToDecl(cursorType, cursor);
    }

    ///
    /// * Primitiveを得る
    /// * 参照型を構築する(Pointer, Reference, Array...)
    /// * User型(Struct, Enum, Typedef)への参照を得る(cursor.children の CXCursorKind._TypeRef から)
    /// * 無名型(Struct)への参照を得る
    /// * Functionの型(struct field, function param/return, typedef)を得る
    ///
    std::shared_ptr<Decl> typeToDecl(const CXType &type, const CXCursor &cursor)
    {
        if (auto primitive = getPrimitive(type.kind))
        {
            return primitive;
        }

        if (type.kind == CXType_Unexposed)
        {
            // nullptr_t
            return std::make_shared<Pointer>(Void::instance(), false);
        }

        if (type.kind == CXType_Pointer)
        {
            auto pointeeType = clang_getPointeeType(type);
            auto isConst = clang_isConstQualifiedType(pointeeType);
            auto pointeeDecl = typeToDecl(pointeeType, cursor);
            if (!pointeeDecl)
            {
                throw "pointer type not found";
            }
            return std::make_shared<Pointer>(pointeeDecl, isConst != 0);
        }

        if (type.kind == CXType_LValueReference)
        {
            auto pointeeType = clang_getPointeeType(type);
            auto isConst = clang_isConstQualifiedType(pointeeType);
            auto pointeeDecl = typeToDecl(pointeeType, cursor);
            if (!pointeeDecl)
            {
                throw "reference type not found";
            }
            return std::make_shared<Reference>(pointeeDecl, isConst != 0);
        }

        if (type.kind == CXType_IncompleteArray)
        {
            auto arrayType = clang_getArrayElementType(type);
            auto isConst = clang_isConstQualifiedType(type);
            auto pointeeDecl = typeToDecl(arrayType, cursor);
            if (!pointeeDecl)
            {
                throw "array[] type not found";
            }
            return std::make_shared<Pointer>(pointeeDecl, isConst != 0);
        }

        if (type.kind == CXType_ConstantArray)
        {
            auto arrayType = clang_getArrayElementType(type);
            auto pointeeDecl = typeToDecl(arrayType, cursor);
            auto arraySize = clang_getArraySize(type);
            if (!pointeeDecl)
            {

                throw "array[x] type not found";
            }
            return std::make_shared<Array>(pointeeDecl, arraySize);
        }

        if (type.kind == CXType_FunctionProto)
        {
            auto resultType = clang_getResultType(type);
            // auto dummy = Context();
            auto decl = parseFunction(cursor, resultType);
            return decl;
        }

        if (type.kind == CXType_Elaborated)
        {
            // tag名無し？
            // 宣言
            std::shared_ptr<Decl> decl;
            processChildren(cursor, [self = this, &decl](const CXCursor &child) {
                switch (child.kind)
                {
                case CXCursor_StructDecl:
                case CXCursor_UnionDecl: {
                    decl = self->getDecl<StructDecl>(child);
                    return CXChildVisit_Break;
                }

                case CXCursor_EnumDecl: {
                    decl = self->getDecl<EnumDecl>(child);
                    return CXChildVisit_Break;
                }

                default:
                    break;
                }
                return CXChildVisit_Continue;
            });
            if (decl)
            {
                // throw std::runtime_error("no decl");
                return decl;
            }
        }

        // 子カーソルから型を得る
        int count = 0;
        {
            std::shared_ptr<Decl> decl;
            processChildren(cursor, [self = this, type, &count, &decl](const CXCursor &child) {
                ++count;
                // auto childKind = cast(CXCursorKind) clang_getCursorKind(child);
                switch (child.kind)
                {

                case CXCursor_TypeRef: {
                    if (type.kind == CXType_Record || type.kind == CXType_Typedef || type.kind == CXType_Elaborated ||
                        type.kind == CXType_Enum)
                    {
                        auto referenced = clang_getCursorReferenced(child);
                        decl = self->getDecl<UserDecl>(referenced);
                        return CXChildVisit_Break;
                    }
                    else
                    {
                        throw new std::runtime_error("not record or typedef");
                    }
                }

                default: {
                    break;
                }
                }

                return CXChildVisit_Continue;
            });

            if (!decl)
            {
                throw std::runtime_error("no decl");
            }

            return decl;
        }
    }

    CXChildVisitResult traverse(const CXCursor &cursor, const Context &context)
    {
        ScopedCXString spelling(clang_getCursorSpelling(cursor));

        // auto location = Location::get(cursor);
        // if (location.file)
        // {
        //     ScopedCXString fileStr(clang_getFileName(file));
        //     LOGD << fileStr.c_str() << ":" << line << ":" << column << ' ' // << ":" << offset
        //          << enum_name_map<CXCursorKind, (int)CXCursor_OverloadCandidate>::get(cursor.kind) << '('
        //          << (int)cursor.kind << ')' << " \"" << spelling.c_str() << "\" ";
        // }
        // else
        // {
        //     LOGD << enum_name_map<CXCursorKind, (int)CXCursor_OverloadCandidate>::get(cursor.kind) << '('
        //          << (int)cursor.kind << ')' << " \"" << spelling.c_str() << "\" ";
        // }

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

        case CXCursor_Namespace: {
            auto child = context.enterNamespace(spelling.str_view());
            TraverseChildren(cursor, child);
        }
        break;

        case CXCursor_UnexposedDecl: {
            ScopedCXTokens tokens(cursor);

            auto child = context.createChild();
            if (tokens.size() >= 2)
            {
                // extern C
                if (tokens.spelling(0).str_view() == "extern" && tokens.spelling(1).str_view() == "\"C\"")
                {
                    child.isExternC = true;
                }
            }

            TraverseChildren(cursor, child);
        }
        break;

        case CXCursor_TypedefDecl:
            parseTypedef(cursor);
            break;

        case CXCursor_FunctionDecl: {
            auto decl = parseFunction(cursor, clang_getCursorResultType(cursor));
            if (decl)
            {
                // auto header = getOrCreateHeader(cursor);
                // header.types ~ = decl;
            }
        }
        break;

        case CXCursor_StructDecl:
        case CXCursor_ClassDecl:
            parseStruct(cursor, context, spelling.str_view(), false);
            break;

        case CXCursor_UnionDecl:
            parseStruct(cursor, context, spelling.str_view(), true);
            break;

        case CXCursor_EnumDecl:
            parseEnum(cursor, spelling.str_view());
            break;

        case CXCursor_VarDecl:
            break;

        default:
            throw std::runtime_error("unknown CXCursorKind");
        }

        return CXChildVisit_Continue;
    }

    void parseTypedef(CXCursor cursor)
    {
        auto hash = clang_hashCursor(cursor);
        auto location = Location::get(cursor);
        ScopedCXString spelling(clang_getCursorSpelling(cursor));
        auto decl = Typedef::create(hash, location.path(), location.line, spelling.str_view());
        pushDecl(cursor, decl);

        if (spelling.str_view() == "CXGlobalOptFlags")
        {
            auto a = 0;
        }

        auto underlying = clang_getTypedefDeclUnderlyingType(cursor);
        auto isConst = clang_isConstQualifiedType(underlying);
        auto type = typeToDecl(underlying, cursor);
        decl->ref = {type, isConst != 0};
    }

    void parseEnum(const CXCursor &cursor, const std::string_view &name)
    {
        auto hash = clang_hashCursor(cursor);
        auto location = Location::get(cursor);
        auto decl = EnumDecl::create(hash, location.path(), location.line, name);
        processChildren(cursor, [&decl](const CXCursor &child) {
            switch (child.kind)
            {
            case CXCursor_EnumConstantDecl: {
                ScopedCXString childName(clang_getCursorSpelling(child));
                auto childValue = clang_getEnumConstantDeclUnsignedValue(child);
                decl->values.emplace_back(
                    EnumValue{std::string(childName.str_view()), static_cast<uint32_t>(childValue)});
            }
            break;

            default:
                throw std::runtime_error("parse enum unknown");
            }

            return CXChildVisit_Continue;
        });

        pushDecl(cursor, decl);
        // auto header = getOrCreateHeader(cursor);
        // header.types ~= decl;
    }

    std::shared_ptr<FunctionDecl> parseFunction(const CXCursor &cursor, const CXType &retType)
    {
        auto hash = clang_hashCursor(cursor);
        auto location = Location::get(cursor);
        ScopedCXString spelling(clang_getCursorSpelling(cursor));
        auto decl = FunctionDecl::create(hash, location.path(), location.line, spelling.str_view());
        // decl->returnType = {retDecl, }
        //, TypeRef(retDecl), params, dllExport, context.isExternC);
        decl->isVariadic = clang_Cursor_isVariadic(cursor) != 0;
        // decl.isVariadic = isVariadic;

        processChildren(cursor, [self = this, &decl](const CXCursor &child) {
            ScopedCXString childName(clang_getCursorSpelling(child));

            switch (child.kind)
            {
            case CXCursor_CompoundStmt:
                decl->hasBody = true;
                break;

            case CXCursor_TypeRef:
                break;

            case CXCursor_WarnUnusedResultAttr:
                break;

            case CXCursor_ParmDecl: {
                // debug
                // {
                //     if (childName == "sourceRectangle")
                //     {
                //         auto a = 0;
                //     }
                //     if (childName == "opacity")
                //     {
                //         auto a = 0;
                //     }
                // }
                auto paramType = self->typeToDecl(child);
                auto paramConst = clang_isConstQualifiedType(clang_getCursorType(child));
                decl->params.emplace_back(FunctionParam{
                    .name = childName.str(), .ref = {paramType, paramConst != 0}
                    // param.values = getDefaultValue(child);
                });
            }
            break;

            case CXCursor_DLLImport:
            case CXCursor_DLLExport:
                decl->dllExport = true;
                break;

            case CXCursor_UnexposedAttr: {
            }
            break;

            default:
                throw std::runtime_error("unknown param type");
            }

            return CXChildVisit_Continue;
        });

        auto retDecl = typeToDecl(retType, cursor);

        // decl.namespace = context.namespace;

        return decl;
    }

    void parseStruct(const CXCursor &cursor, const Context &context, const std::string_view &name, bool isUnion)
    {
        auto decl = getDecl<StructDecl>(cursor);
        if (!decl)
        {
            // first time
            auto hash = clang_hashCursor(cursor);
            auto location = Location::get(cursor);
            decl = StructDecl::create(hash, location.path(), location.line, name);
            pushDecl(cursor, decl);
        }

        // decl.namespace = context.namespace;
        decl->isUnion = isUnion;
        decl->isForwardDecl = isForwardDeclaration(cursor);
        if (decl->isForwardDecl)
        {
            auto defCursor = clang_getCursorDefinition(cursor);
            if (clang_equalCursors(defCursor, clang_getNullCursor()))
            {
                // not exists
            }
            else
            {
                auto defDecl = getDecl<StructDecl>(defCursor);
                if (!defDecl)
                {
                    // create
                    auto defHash = clang_hashCursor(defCursor);
                    auto defLocation = Location::get(defCursor);
                    ScopedCXString defSpelling(clang_getCursorSpelling(defCursor));
                    defDecl = StructDecl::create(defHash, defLocation.path(), defLocation.line, defSpelling.str_view());
                    pushDecl(defCursor, defDecl);
                }
                decl->definition = defDecl;
            }
        }

        // push before fields
        // auto header = getOrCreateHeader(cursor);
        // header.types ~ = decl;

        // fields
        auto childContext = context.enterNamespace(name);
        processChildren(cursor,
                        std::bind(&TraverserImpl::parseStructField, this, decl, std::placeholders::_1, childContext));
    }

    CXChildVisitResult parseStructField(const std::shared_ptr<StructDecl> &structDecl, const CXCursor &child,
                                        const Context &context)
    {
        switch (child.kind)
        {
        case CXCursor_FieldDecl: {
            ScopedCXString fieldName(clang_getCursorSpelling(child));
            auto fieldOffset = clang_Cursor_getOffsetOfField(child);
            auto fieldDecl = typeToDecl(child);
            auto fieldType = clang_getCursorType(child);
            auto fieldConst = clang_isConstQualifiedType(fieldType);
            structDecl->fields.emplace_back(StructField{
                .offset = (uint32_t)fieldOffset, .name = fieldName.str(), .ref = {fieldDecl, fieldConst != 0}});
            break;
        }

        case CXCursor_UnexposedAttr:
            // {
            //     auto src = getSource(child);
            //     auto uuid = getUUID(src);
            //     if (!uuid.empty())
            //     {
            //         decl.iid = uuid;
            //     }
            // }
            break;

        case CXCursor_CXXMethod: {
            auto method = parseFunction(child, clang_getCursorResultType(child));
            if (!method->hasBody)
            {
                // CXCursor *p;
                // uint32_t n;
                // ulong[] hashes;
                // clang_getOverriddenCursors(child, &p, &n);
                // if (n)
                // {
                //     scope(exit) clang_disposeOverriddenCursors(p);

                //     hashes.length = n;
                //     for (int i = 0; i < n; ++i)
                //     {
                //         hashes[i] = clang_hashCursor(p[i]);
                //     }

                //     debug
                //     {
                //         auto childName = getCursorSpelling(child);
                //         auto a = 0;
                //     }
                // }

                // auto found = -1;
                // for (int i = 0; i < decl.vtable.length; ++i)
                // {
                //     auto current = decl.vtable[i].hash;
                //     if (hashes.any !(x = > x == current))
                //     {
                //         found = i;
                //         break;
                //     }
                // }
                // if (found != -1)
                // {
                //     debug auto a = 0;
                // }
                // else
                // {
                //     found = cast(int) decl.vtable.length;
                //     decl.vtable ~ = method;
                //     debug auto a = 0;
                // }
                // decl.methodVTableIndices ~ = found;
                // decl.methods ~ = method;
            }
        }
        break;

        case CXCursor_Constructor:
        case CXCursor_Destructor:
        case CXCursor_ConversionFunction:
        case CXCursor_FunctionTemplate:
        case CXCursor_VarDecl:
        case CXCursor_ObjCClassMethodDecl:
        case CXCursor_UnexposedExpr:
        case CXCursor_AlignedAttr:
        case CXCursor_CXXAccessSpecifier:
            break;

        case CXCursor_CXXBaseSpecifier: {
            // Decl referenced = getReferenceType(child);
            // while (true)
            // {
            //     auto typeDef = cast(TypeDef) referenced;
            //     if (!typeDef)
            //         break;
            //     referenced = typeDef.typeref.type;
            // }
            // auto base = cast(Struct) referenced;
            // if (base.definition)
            // {
            //     base = base.definition;
            // }
            // decl.base = base;
            // decl.vtable = base.vtable;
        }
        break;

        case CXCursor_TypeRef:
            // template param ?
            // debug auto a = 0;
            break;

            // case CXCursor_StructDecl:
            // case CXCursor_ClassDecl:
            // case CXCursor_UnionDecl:
            // case CXCursor_TypedefDecl:
            // case CXCursor_EnumDecl: {
            //     // nested type
            //     traverse(child, context);
            //     // auto nestName = getCursorSpelling(child);
            //     // if (nestName == "")
            //     // {
            //     //     // anonymous
            //     //     auto fieldOffset = clang_Cursor_getOffsetOfField(child);
            //     //     auto fieldDecl = getDeclFromCursor(child);
            //     //     auto fieldConst = clang_isConstQualifiedType(fieldType);
            //     //     decl.fields ~ = Field(fieldOffset, nestName, TypeRef(fieldDecl, fieldConst != 0));
            //     // }
            // }
            // break;

        default:
            traverse(child, context);
            break;
        }

        return CXChildVisit_Continue;
    }
};

void ClangCursorTraverser::Traverse(const CXCursor &cursor)
{
    TraverserImpl impl;
    impl.TraverseChildren(cursor, {});
}

} // namespace clalua