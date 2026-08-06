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

class CLuaOcclusionDefs : public CLuaDefs
{
public:
    static void LoadFunctions();
    static void AddClass(lua_State* luaVM);

    static CClientOcclusion* CreateOcclusion(lua_State* luaVM, CVector vecPosition, CVector vecSize, CVector vecRotation, std::optional<bool> interior);

    static bool SetOcclusionSize(CClientOcclusion* pOcclusion, CVector vecSize);
    static bool SetOcclusionRotation(CClientOcclusion* pOcclusion, CVector vecRotation);
    static bool SetOcclusionEnabled(CClientOcclusion* pOcclusion, bool bEnabled);

    static std::variant<bool, CLuaMultiReturn<float, float, float>> GetOcclusionSize(CClientOcclusion* pOcclusion);
    static std::variant<bool, CLuaMultiReturn<float, float, float>> GetOcclusionRotation(CClientOcclusion* pOcclusion);
    static bool                                                     IsOcclusionEnabled(CClientOcclusion* pOcclusion);
    static bool                                                     IsOcclusionNative(CClientOcclusion* pOcclusion);
    static bool                                                     IsOcclusionInInterior(CClientOcclusion* pOcclusion);
};
