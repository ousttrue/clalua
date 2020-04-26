#pragma once

#ifdef CLALUA_BUILD
#define CLALUA_EXPORT __declspec(dllexport)
#else
#define CLALUA_EXPORT __declspec(dllimport)
#endif

extern "C"
{
#include <lua.h>

    CLALUA_EXPORT int luaopen_clalua(lua_State *L);
}
