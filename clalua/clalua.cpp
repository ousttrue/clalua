#include "clalua.h"
#include "ClangIndex.h"
#include "ClangCursorTraverser.h"
#include "ClangDeclProcessor.h"
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Log.h>
#include <string>
#include <vector>
#include <perilune/perilune.h>

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace plog
{
class MyFormatter
{
public:
    static util::nstring header() // This method returns a header for a new file. In our case it is empty.
    {
        return util::nstring();
    }

    static util::nstring format(const Record &record) // This method returns a string from a record.
    {
        util::nostringstream ss;
        ss << record.getMessage() << "\n"; // Produce a simple string with a log message.

        return ss.str();
    }
};
} // namespace plog

struct Lua
{
    lua_State *L;

    Lua() : L(luaL_newstate())
    {
        luaL_requiref(L, "_G", luaopen_base, 1);
        lua_pop(L, 1);
    }

    ~Lua()
    {
        lua_close(L);
    }

    void printLuaError()
    {
        std::cerr << lua_tostring(L, -1) << std::endl;
    }

    bool doFile(const char *file)
    {
        if (luaL_dofile(L, file))
        {
            printLuaError();
            return false;
        }
        return true;
    }

    void cmdline(int argc, char **argv)
    {
        if (argc < 2)
        {
            // error
            LOGE << "usage: clalua.exe {script.lua} [args...]";
            return;
        }

        auto file = argv[1];
        argv += 2;
        argc -= 2;

        // parse script
        auto chunk = luaL_loadfile(L, file);
        if (chunk)
        {
            // error
            LOGE << lua_tostring(L, -1);
            return;
        }

        // push arguments
        for (int i = 0; i < argc; ++i)
        {
            lua_pushstring(L, argv[i]);
        }

        // execute chunk
        auto result = lua_pcall(L, argc, LUA_MULTRET, 0);
        if (result)
        {
            LOGE << lua_tostring(L, -1);
            return;
        }
    }
};

static void BackslashToSlash(std::string &src)
{
    for (auto &c : src)
    {
        if (c == '\\')
        {
            c = (char)'/';
        }
    }
}

static void BackslashStoSlash(std::vector<std::string> &srcs)
{
    for (auto &src : srcs)
    {
        BackslashToSlash(src);
    }
}

static void PushDecl(lua_State *L, const std::shared_ptr<clalua::Decl> &decl);

static void PushRef(lua_State *L, const clalua::TypeReference &ref)
{
    lua_newtable(L);

    lua_pushstring(L, "type");
    PushDecl(L, ref.decl);
    lua_settable(L, -3);
}

static void PushTypedefDecl(lua_State *L, const std::shared_ptr<clalua::Typedef> &decl)
{
    lua_pushstring(L, "class");
    lua_pushstring(L, "TypeDef");
    lua_settable(L, -3);

    lua_pushstring(L, "ref");
    PushRef(L, decl->ref);
    lua_settable(L, -3);
}

static void PushEnumDecl(lua_State *L, const std::shared_ptr<clalua::EnumDecl> &decl)
{
    lua_pushstring(L, "class");
    lua_pushstring(L, "Enum");
    lua_settable(L, -3);

    lua_pushstring(L, "values");
    lua_newtable(L);
    // TODO:
    lua_settable(L, -3);
}

static void PushField(lua_State *L, const clalua::StructField &field)
{
    lua_newtable(L);

    // name
    lua_pushstring(L, "name");
    lua_pushstring(L, field.name.c_str());
    lua_settable(L, -3);

    // offset
    lua_pushstring(L, "offset");
    lua_pushinteger(L, field.offset);
    lua_settable(L, -3);

    // ref
    lua_pushstring(L, "ref");
    PushRef(L, field.ref);
    lua_settable(L, -3);
}

static void PushStructDecl(lua_State *L, const std::shared_ptr<clalua::StructDecl> &decl)
{
    lua_pushstring(L, "class");
    lua_pushstring(L, "Struct");
    lua_settable(L, -3);

    lua_pushstring(L, "namespace");
    lua_newtable(L);
    // TODO:
    lua_settable(L, -3);

    lua_pushstring(L, "fields");
    lua_newtable(L);
    // int i = 1;
    // for (auto &field : decl->fields)
    // {
    //     PushField(L, field);
    //     lua_rawseti(L, -2, i++);
    // }
    lua_settable(L, -3);
}

static void PushFunctionDecl(lua_State *L, const std::shared_ptr<clalua::FunctionDecl> &decl)
{
    lua_pushstring(L, "class");
    lua_pushstring(L, "Function");
    lua_settable(L, -3);
}

static void PushUserDecl(lua_State *L, const std::shared_ptr<clalua::UserDecl> &decl)
{
    lua_newtable(L);

    lua_pushstring(L, "name");
    lua_pushstring(L, decl->name.c_str());
    lua_settable(L, -3);

    lua_pushstring(L, "hash");
    lua_pushinteger(L, decl->hash);
    lua_settable(L, -3);

    lua_pushstring(L, "useCount");
    // TODO:
    lua_pushinteger(L, 0);
    lua_settable(L, -3);

    if (auto typedefDecl = std::dynamic_pointer_cast<clalua::Typedef>(decl))
    {
        PushTypedefDecl(L, typedefDecl);
    }
    else if (auto enumDecl = std::dynamic_pointer_cast<clalua::EnumDecl>(decl))
    {
        PushEnumDecl(L, enumDecl);
    }
    else if (auto structDecl = std::dynamic_pointer_cast<clalua::StructDecl>(decl))
    {
        PushStructDecl(L, structDecl);
    }
    else if (auto functionDecl = std::dynamic_pointer_cast<clalua::FunctionDecl>(decl))
    {
        // PushFunctionDecl(L, functionDecl);
    }
    else
    {
        std::cout << "unknown UserDecl: " << decl->name << std::endl;
    }
}

template <typename T>
bool PushPrim(lua_State *L, const std::shared_ptr<clalua::Primitive> &decl)
{
    auto t = std::dynamic_pointer_cast<T>(decl);
    if (!t)
    {
        return false;
    }

    lua_newtable(L);

    lua_pushstring(L, "name");
    lua_pushstring(L, typeid(T).name());
    lua_settable(L, -3);

    return true;
}

static void PushDecl(lua_State *L, const std::shared_ptr<clalua::Decl> &decl)
{
    if (auto userDecl = std::dynamic_pointer_cast<clalua::UserDecl>(decl))
    {
        PushUserDecl(L, userDecl);
    }
    else if (auto primitive = std::dynamic_pointer_cast<clalua::Primitive>(decl))
    {
        if (PushPrim<clalua::Void>(L, primitive))
            return;
        else if (PushPrim<clalua::Bool>(L, primitive))
            return;
        else if (PushPrim<clalua::Int8>(L, primitive))
            return;
        else if (PushPrim<clalua::Int16>(L, primitive))
            return;
        else if (PushPrim<clalua::Int32>(L, primitive))
            return;
        else if (PushPrim<clalua::Int64>(L, primitive))
            return;
        else if (PushPrim<clalua::UInt8>(L, primitive))
            return;
        else if (PushPrim<clalua::UInt16>(L, primitive))
            return;
        else if (PushPrim<clalua::UInt32>(L, primitive))
            return;
        else if (PushPrim<clalua::UInt64>(L, primitive))
            return;
        else if (PushPrim<clalua::Float>(L, primitive))
            return;
        else if (PushPrim<clalua::Double>(L, primitive))
            return;
        else
        {
            throw "unknown primitive";
        }
    }
    else if (auto pointer = std::dynamic_pointer_cast<clalua::Pointer>(decl))
    {
        lua_newtable(L);
        lua_pushstring(L, "class");
        lua_pushstring(L, "Pointer");
        lua_settable(L, -3);

        lua_pushstring(L, "ref");
        PushRef(L, pointer->pointee);
        lua_settable(L, -3);
    }
    else if (auto reference = std::dynamic_pointer_cast<clalua::Reference>(decl))
    {
        lua_newtable(L);
        lua_pushstring(L, "class");
        lua_pushstring(L, "Reference");
        lua_settable(L, -3);
    }
    else if (auto array = std::dynamic_pointer_cast<clalua::Array>(decl))
    {
        lua_newtable(L);
        lua_pushstring(L, "class");
        lua_pushstring(L, "Array");
        lua_settable(L, -3);
    }
    else
    {
        // lua_pushstring(L, "__unknown__");
        std::cout << "unknown: " << typeid(decl).name() << std::endl;
    }
}

// return {decls, macros}
static int PushSource(lua_State *L, const clalua::SourcePtr &source)
{
    auto top = lua_gettop(L);
    lua_newtable(L);

    {
        lua_pushstring(L, "name");
        lua_pushstring(L, source->Name().c_str());
        lua_settable(L, -3);
    }

    // decls
    {
        lua_pushstring(L, "types");
        lua_newtable(L);
        {
            int i = 1;
            for (auto decl : source->Decls)
            {
                PushDecl(L, decl);
                lua_rawseti(L, -2, i++);
            }
        }
        lua_settable(L, -3);
    }

    // macros
    {
        lua_pushstring(L, "macros");
        lua_newtable(L);
        lua_settable(L, -3);
    }

    // std::cerr << "top: " << (lua_gettop(L) - top) << std::endl;
    assert(lua_gettop(L) - top == 1);
    return 1;
}

int CLALUA_parse(lua_State *L)
{
    // 型情報を集める
    auto headers = perilune::LuaGetVector<std::string>(L, 1);
    auto includes = perilune::LuaGetVector<std::string>(L, 2);
    auto defines = perilune::LuaGetVector<std::string>(L, 3);
    auto externC = perilune::LuaGet<bool>::Get(L, 4);

    std::unordered_map<uint32_t, std::shared_ptr<clalua::UserDecl>> map = clalua::Parse(headers, includes, defines);
    if (map.empty())
    {
        return 0;
    }

    clalua::ClangDeclProcessor processor;
    for (auto [id, decl] : map)
    {
        auto found = std::find(headers.begin(), headers.end(), decl->path);
        if (found != headers.end())
        {
            processor.AddDecl(decl, {});
        }
    }

    //
    // return map<path, source>
    //
    lua_newtable(L);

    for (auto [key, value] : processor.SourceMap)
    {
        // std::cout << key << ": " << value->Decls.size() << ::std::endl;
        lua_pushstring(L, key.c_str());
        PushSource(L, value);
        lua_settable(L, -3);
    }

    return 1;
}

int luaopen_clalua(lua_State *L)
{
    lua_newtable(L);

    lua_pushcfunction(L, CLALUA_parse);
    lua_setfield(L, -2, "parse");

    // type

    return 1;
}
