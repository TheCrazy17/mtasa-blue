/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/luadefs/CLuaGlassPanelDefs.h
 *  PURPOSE:     Lua definitions class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once
#include "CLuaDefs.h"
#include <lua/CLuaFunctionParser.h>
#include <lua/CLuaMultiReturn.h>

class CLuaGlassPanelDefs : public CLuaDefs
{
public:
    static void LoadFunctions();
    static void AddClass(lua_State* luaVM);

    static std::variant<CClientGlassPanel*, bool> CreateGlassPanel(lua_State* luaVM, CVector vecPosition, float fWidth, float fHeight);

    static bool                          SetGlassPanelSize(CClientGlassPanel* pPanel, float fWidth, float fHeight);
    static CLuaMultiReturn<float, float> GetGlassPanelSize(CClientGlassPanel* pPanel);

    static bool  SetGlassPanelThickness(CClientGlassPanel* pPanel, float fThickness);
    static float GetGlassPanelThickness(CClientGlassPanel* pPanel);

    static bool SetGlassColor(CClientGlassPanel* pPanel, uchar ucRed, uchar ucGreen, uchar ucBlue, std::optional<uchar> ucAlpha);
    static CLuaMultiReturn<uchar, uchar, uchar, uchar> GetGlassColor(CClientGlassPanel* pPanel);

    static bool SetGlassPanelBreakable(CClientGlassPanel* pPanel, bool bBreakable);
    static bool IsGlassPanelBreakable(CClientGlassPanel* pPanel);
    static bool IsGlassPanelBroken(CClientGlassPanel* pPanel);
    static bool BreakGlassPanel(CClientGlassPanel* pPanel, std::optional<CVector> vecForce, std::optional<uchar> ucGranularity);

    static bool  SetGlassPanelMaxDamage(CClientGlassPanel* pPanel, uchar ucMaxDamage);
    static uchar GetGlassPanelMaxDamage(CClientGlassPanel* pPanel);
    static uchar GetGlassPanelDamage(CClientGlassPanel* pPanel);
    static bool  DamageGlassPanel(CClientGlassPanel* pPanel, std::optional<uchar> ucAmount, std::optional<CVector> vecForce,
                                  std::optional<uchar> ucGranularity);

    static bool SetGlassPanelCollisionEnabled(CClientGlassPanel* pPanel, bool bEnabled);
    static bool IsGlassPanelCollisionEnabled(CClientGlassPanel* pPanel);
};
