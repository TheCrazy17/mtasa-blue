/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/luadefs/CLuaOcclusionDefs.cpp
 *  PURPOSE:     Lua definitions class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <lua/CLuaFunctionParser.h>

void CLuaOcclusionDefs::LoadFunctions()
{
    constexpr static const std::pair<const char*, lua_CFunction> functions[]{
        {"createOcclusion", ArgumentParser<CreateOcclusion>},

        {"setOcclusionSize", ArgumentParser<SetOcclusionSize>},
        {"setOcclusionRotation", ArgumentParser<SetOcclusionRotation>},
        {"setOcclusionEnabled", ArgumentParser<SetOcclusionEnabled>},

        {"getOcclusionSize", ArgumentParser<GetOcclusionSize>},
        {"getOcclusionRotation", ArgumentParser<GetOcclusionRotation>},
        {"isOcclusionEnabled", ArgumentParser<IsOcclusionEnabled>},
        {"isOcclusionNative", ArgumentParser<IsOcclusionNative>},
        {"isOcclusionInInterior", ArgumentParser<IsOcclusionInInterior>},
    };

    for (const auto& [name, func] : functions)
        CLuaCFunctions::AddFunction(name, func);
}

void CLuaOcclusionDefs::AddClass(lua_State* luaVM)
{
    lua_newclass(luaVM);

    lua_classfunction(luaVM, "create", "createOcclusion");

    lua_classfunction(luaVM, "getSize", "getOcclusionSize");
    lua_classfunction(luaVM, "getRotation", "getOcclusionRotation");
    lua_classfunction(luaVM, "isEnabled", "isOcclusionEnabled");
    lua_classfunction(luaVM, "isNative", "isOcclusionNative");
    lua_classfunction(luaVM, "isInInterior", "isOcclusionInInterior");

    lua_classfunction(luaVM, "setSize", "setOcclusionSize");
    lua_classfunction(luaVM, "setRotation", "setOcclusionRotation");
    lua_classfunction(luaVM, "setEnabled", "setOcclusionEnabled");

    lua_classvariable(luaVM, "size", "setOcclusionSize", "getOcclusionSize");
    lua_classvariable(luaVM, "rotation", "setOcclusionRotation", "getOcclusionRotation");
    lua_classvariable(luaVM, "enabled", "setOcclusionEnabled", "isOcclusionEnabled");
    lua_classvariable(luaVM, "native", nullptr, "isOcclusionNative");
    lua_classvariable(luaVM, "inInterior", nullptr, "isOcclusionInInterior");

    lua_registerclass(luaVM, "Occlusion", "Element");
}

CClientOcclusion* CLuaOcclusionDefs::CreateOcclusion(lua_State* luaVM, CVector vecPosition, CVector vecSize, CVector vecRotation, std::optional<bool> interior)
{
    CLuaMain*  pLuaMain = m_pLuaManager->GetVirtualMachine(luaVM);
    CResource* pResource = pLuaMain ? pLuaMain->GetResource() : nullptr;
    if (!pResource)
        return nullptr;

    return CStaticFunctionDefinitions::CreateOcclusionZone(*pResource, vecPosition, vecSize, vecRotation, interior.value_or(false));
}

bool CLuaOcclusionDefs::SetOcclusionSize(CClientOcclusion* pOcclusion, CVector vecSize)
{
    return CStaticFunctionDefinitions::SetOcclusionZoneSize(pOcclusion, vecSize);
}

bool CLuaOcclusionDefs::SetOcclusionRotation(CClientOcclusion* pOcclusion, CVector vecRotation)
{
    return CStaticFunctionDefinitions::SetOcclusionZoneRotation(pOcclusion, vecRotation);
}

bool CLuaOcclusionDefs::SetOcclusionEnabled(CClientOcclusion* pOcclusion, bool bEnabled)
{
    return CStaticFunctionDefinitions::SetOcclusionZoneEnabled(pOcclusion, bEnabled);
}

std::variant<bool, CLuaMultiReturn<float, float, float>> CLuaOcclusionDefs::GetOcclusionSize(CClientOcclusion* pOcclusion)
{
    CVector vecSize;
    if (!CStaticFunctionDefinitions::GetOcclusionZoneSize(pOcclusion, vecSize))
        return false;

    return std::tuple(vecSize.fX, vecSize.fY, vecSize.fZ);
}

std::variant<bool, CLuaMultiReturn<float, float, float>> CLuaOcclusionDefs::GetOcclusionRotation(CClientOcclusion* pOcclusion)
{
    CVector vecRotation;
    if (!CStaticFunctionDefinitions::GetOcclusionZoneRotation(pOcclusion, vecRotation))
        return false;

    return std::tuple(vecRotation.fX, vecRotation.fY, vecRotation.fZ);
}

bool CLuaOcclusionDefs::IsOcclusionEnabled(CClientOcclusion* pOcclusion)
{
    return pOcclusion->IsEnabled();
}

bool CLuaOcclusionDefs::IsOcclusionNative(CClientOcclusion* pOcclusion)
{
    return pOcclusion->IsNative();
}

bool CLuaOcclusionDefs::IsOcclusionInInterior(CClientOcclusion* pOcclusion)
{
    return pOcclusion->IsInterior();
}
