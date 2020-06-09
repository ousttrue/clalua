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

struct Source
{
    std::string Path;
    std::vector<std::shared_ptr<clalua::UserDecl>> Declarations;
};
using SourcePtr = std::shared_ptr<Source>;

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

    lua_newtable(L);

    for (auto [key, value] : processor.SourceMap)
    {
        std::cout << key << ": " << value->Decls.size();
        lua_pushstring(L, key.c_str());

        {
            lua_newtable(L);
            int i = 1;
            for (auto decl : value->Decls)
            {
                lua_pushstring(L, decl->name.c_str());
                lua_rawseti(L, -2, i++);
            }
        }

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
