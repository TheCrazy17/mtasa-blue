/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/luadefs/CLuaGlassPanelDefs.cpp
 *  PURPOSE:     Lua definitions class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

void CLuaGlassPanelDefs::LoadFunctions()
{
    constexpr static const std::pair<const char*, lua_CFunction> functions[]{
        {"createGlassPanel", ArgumentParser<CreateGlassPanel>},
        {"setGlassPanelSize", ArgumentParser<SetGlassPanelSize>},
        {"getGlassPanelSize", ArgumentParser<GetGlassPanelSize>},
        {"setGlassPanelThickness", ArgumentParser<SetGlassPanelThickness>},
        {"getGlassPanelThickness", ArgumentParser<GetGlassPanelThickness>},
        {"setGlassColor", ArgumentParser<SetGlassColor>},
        {"getGlassColor", ArgumentParser<GetGlassColor>},
        {"setGlassPanelBreakable", ArgumentParser<SetGlassPanelBreakable>},
        {"isGlassPanelBreakable", ArgumentParser<IsGlassPanelBreakable>},
        {"isGlassPanelBroken", ArgumentParser<IsGlassPanelBroken>},
        {"breakGlassPanel", ArgumentParser<BreakGlassPanel>},
        {"setGlassPanelMaxDamage", ArgumentParser<SetGlassPanelMaxDamage>},
        {"getGlassPanelMaxDamage", ArgumentParser<GetGlassPanelMaxDamage>},
        {"getGlassPanelDamage", ArgumentParser<GetGlassPanelDamage>},
        {"damageGlassPanel", ArgumentParser<DamageGlassPanel>},
        {"setGlassPanelCollisionEnabled", ArgumentParser<SetGlassPanelCollisionEnabled>},
        {"isGlassPanelCollisionEnabled", ArgumentParser<IsGlassPanelCollisionEnabled>},
    };

    for (const auto& [name, func] : functions)
        CLuaCFunctions::AddFunction(name, func);
}

void CLuaGlassPanelDefs::AddClass(lua_State* luaVM)
{
    lua_newclass(luaVM);

    lua_classfunction(luaVM, "create", "createGlassPanel");

    lua_classfunction(luaVM, "setSize", "setGlassPanelSize");
    lua_classfunction(luaVM, "setThickness", "setGlassPanelThickness");
    lua_classfunction(luaVM, "setColor", "setGlassColor");
    lua_classfunction(luaVM, "setBreakable", "setGlassPanelBreakable");
    lua_classfunction(luaVM, "setCollisionEnabled", "setGlassPanelCollisionEnabled");
    lua_classfunction(luaVM, "break", "breakGlassPanel");
    lua_classfunction(luaVM, "setMaxDamage", "setGlassPanelMaxDamage");
    lua_classfunction(luaVM, "damage", "damageGlassPanel");

    lua_classfunction(luaVM, "getSize", "getGlassPanelSize");
    lua_classfunction(luaVM, "getThickness", "getGlassPanelThickness");
    lua_classfunction(luaVM, "getColor", "getGlassColor");
    lua_classfunction(luaVM, "isBreakable", "isGlassPanelBreakable");
    lua_classfunction(luaVM, "isBroken", "isGlassPanelBroken");
    lua_classfunction(luaVM, "getMaxDamage", "getGlassPanelMaxDamage");
    lua_classfunction(luaVM, "getDamage", "getGlassPanelDamage");
    lua_classfunction(luaVM, "isCollisionEnabled", "isGlassPanelCollisionEnabled");

    lua_classvariable(luaVM, "thickness", "setGlassPanelThickness", "getGlassPanelThickness");
    lua_classvariable(luaVM, "breakable", "setGlassPanelBreakable", "isGlassPanelBreakable");
    lua_classvariable(luaVM, "broken", nullptr, "isGlassPanelBroken");
    lua_classvariable(luaVM, "maxDamage", "setGlassPanelMaxDamage", "getGlassPanelMaxDamage");
    lua_classvariable(luaVM, "damage", nullptr, "getGlassPanelDamage");
    lua_classvariable(luaVM, "collisionEnabled", "setGlassPanelCollisionEnabled", "isGlassPanelCollisionEnabled");

    lua_registerclass(luaVM, "GlassPanel", "Element");
}

std::variant<CClientGlassPanel*, bool> CLuaGlassPanelDefs::CreateGlassPanel(lua_State* luaVM, CVector vecPosition, float fWidth, float fHeight)
{
    CResource&          resource = lua_getownerresource(luaVM);
    CClientGlassPanel* pPanel = CStaticFunctionDefinitions::CreateGlassPanel(resource, vecPosition, fWidth, fHeight);
    if (!pPanel)
        return false;

    if (CElementGroup* elementGroup = resource.GetElementGroup())
        elementGroup->Add(pPanel);

    return pPanel;
}

bool CLuaGlassPanelDefs::SetGlassPanelSize(CClientGlassPanel* pPanel, float fWidth, float fHeight)
{
    return CStaticFunctionDefinitions::SetGlassPanelSize(*pPanel, fWidth, fHeight);
}

CLuaMultiReturn<float, float> CLuaGlassPanelDefs::GetGlassPanelSize(CClientGlassPanel* pPanel)
{
    float fWidth, fHeight;
    pPanel->GetSize(fWidth, fHeight);
    return {fWidth, fHeight};
}

bool CLuaGlassPanelDefs::SetGlassPanelThickness(CClientGlassPanel* pPanel, float fThickness)
{
    return CStaticFunctionDefinitions::SetGlassPanelThickness(*pPanel, fThickness);
}

float CLuaGlassPanelDefs::GetGlassPanelThickness(CClientGlassPanel* pPanel)
{
    return pPanel->GetThickness();
}

bool CLuaGlassPanelDefs::SetGlassColor(CClientGlassPanel* pPanel, uchar ucRed, uchar ucGreen, uchar ucBlue, std::optional<uchar> ucAlpha)
{
    return CStaticFunctionDefinitions::SetGlassPanelColor(*pPanel, SColorRGBA(ucRed, ucGreen, ucBlue, ucAlpha.value_or(255)));
}

CLuaMultiReturn<uchar, uchar, uchar, uchar> CLuaGlassPanelDefs::GetGlassColor(CClientGlassPanel* pPanel)
{
    const SColor color = pPanel->GetColor();
    return {color.R, color.G, color.B, color.A};
}

bool CLuaGlassPanelDefs::SetGlassPanelBreakable(CClientGlassPanel* pPanel, bool bBreakable)
{
    return CStaticFunctionDefinitions::SetGlassPanelBreakable(*pPanel, bBreakable);
}

bool CLuaGlassPanelDefs::IsGlassPanelBreakable(CClientGlassPanel* pPanel)
{
    return pPanel->IsBreakable();
}

bool CLuaGlassPanelDefs::IsGlassPanelBroken(CClientGlassPanel* pPanel)
{
    return pPanel->IsBroken();
}

bool CLuaGlassPanelDefs::BreakGlassPanel(CClientGlassPanel* pPanel, std::optional<CVector> vecForce, std::optional<uchar> ucGranularity)
{
    return CStaticFunctionDefinitions::BreakGlassPanel(*pPanel, vecForce.value_or(CVector()), ucGranularity.value_or(2));
}

bool CLuaGlassPanelDefs::SetGlassPanelMaxDamage(CClientGlassPanel* pPanel, uchar ucMaxDamage)
{
    return CStaticFunctionDefinitions::SetGlassPanelMaxDamage(*pPanel, ucMaxDamage);
}

uchar CLuaGlassPanelDefs::GetGlassPanelMaxDamage(CClientGlassPanel* pPanel)
{
    return pPanel->GetMaxDamage();
}

uchar CLuaGlassPanelDefs::GetGlassPanelDamage(CClientGlassPanel* pPanel)
{
    return pPanel->GetDamage();
}

bool CLuaGlassPanelDefs::DamageGlassPanel(CClientGlassPanel* pPanel, std::optional<uchar> ucAmount, std::optional<CVector> vecForce,
                                          std::optional<uchar> ucGranularity)
{
    return CStaticFunctionDefinitions::DamageGlassPanel(*pPanel, ucAmount.value_or(1), vecForce.value_or(CVector()), ucGranularity.value_or(2));
}

bool CLuaGlassPanelDefs::SetGlassPanelCollisionEnabled(CClientGlassPanel* pPanel, bool bEnabled)
{
    return CStaticFunctionDefinitions::SetGlassPanelCollisionEnabled(*pPanel, bEnabled);
}

bool CLuaGlassPanelDefs::IsGlassPanelCollisionEnabled(CClientGlassPanel* pPanel)
{
    return pPanel->IsCollisionEnabled();
}
