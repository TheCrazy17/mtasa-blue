/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/luadefs/CLuaOcclusionDefs.h
 *  PURPOSE:     Lua definitions class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once
#include "CLuaDefs.h"
#include <lua/CLuaMultiReturn.h>
#include <cstdint>
#include <variant>

class CLuaOcclusionDefs : public CLuaDefs
{
public:
    static void LoadFunctions();
    static void AddClass(lua_State* luaVM);

    static CClientOcclusion* CreateOcclusion(lua_State* luaVM, CVector vecPosition, CVector vecSize, CVector vecRotation, std::optional<bool> interior);

    static bool                                                     SetOcclusionSize(CClientOcclusion* pOcclusion, CVector vecSize);
    static std::variant<bool, CLuaMultiReturn<float, float, float>> GetOcclusionSize(CClientOcclusion* pOcclusion);

    // Accepts either a custom Occlusion element or a native zone id from getOcclusions.
    static bool SetOcclusionEnabled(lua_State* luaVM, std::variant<CClientOcclusion*, std::uint32_t> target, bool bEnabled);
    static bool IsOcclusionEnabled(std::variant<CClientOcclusion*, std::uint32_t> target);

    // Not ArgumentParser-wrapped; builds a table of tables directly on the Lua stack.
    static int GetOcclusions(lua_State* luaVM);
};
