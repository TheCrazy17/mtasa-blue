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

        {"setOcclusionSize", ArgumentParser<SetOcclusionSize>}, {"setOcclusionEnabled", ArgumentParser<SetOcclusionEnabled>},

        {"getOcclusionSize", ArgumentParser<GetOcclusionSize>}, {"isOcclusionEnabled", ArgumentParser<IsOcclusionEnabled>},   {"getOcclusions", GetOcclusions},
    };

    for (const auto& [name, func] : functions)
        CLuaCFunctions::AddFunction(name, func);
}

void CLuaOcclusionDefs::AddClass(lua_State* luaVM)
{
    lua_newclass(luaVM);

    lua_classfunction(luaVM, "create", "createOcclusion");

    lua_classfunction(luaVM, "getSize", "getOcclusionSize");
    lua_classfunction(luaVM, "isEnabled", "isOcclusionEnabled");

    lua_classfunction(luaVM, "setSize", "setOcclusionSize");
    lua_classfunction(luaVM, "setEnabled", "setOcclusionEnabled");

    lua_classvariable(luaVM, "size", "setOcclusionSize", "getOcclusionSize");
    lua_classvariable(luaVM, "enabled", "setOcclusionEnabled", "isOcclusionEnabled");

    // Position and rotation come from the Element class; CClientOcclusion overrides
    // GetPosition/SetPosition and GetRotationDegrees/SetRotationDegrees for those.
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
    return CStaticFunctionDefinitions::SetOcclusionSize(pOcclusion, vecSize);
}

std::variant<bool, CLuaMultiReturn<float, float, float>> CLuaOcclusionDefs::GetOcclusionSize(CClientOcclusion* pOcclusion)
{
    CVector vecSize;
    if (!CStaticFunctionDefinitions::GetOcclusionSize(pOcclusion, vecSize))
        return false;

    return std::tuple(vecSize.fX, vecSize.fY, vecSize.fZ);
}

bool CLuaOcclusionDefs::SetOcclusionEnabled(lua_State* luaVM, std::variant<CClientOcclusion*, std::uint32_t> target, bool bEnabled)
{
    if (std::holds_alternative<CClientOcclusion*>(target))
        return CStaticFunctionDefinitions::SetOcclusionEnabled(std::get<CClientOcclusion*>(target), bEnabled);

    CLuaMain*  pLuaMain = m_pLuaManager->GetVirtualMachine(luaVM);
    CResource* pResource = pLuaMain ? pLuaMain->GetResource() : nullptr;
    if (!pResource)
        return false;

    return CStaticFunctionDefinitions::SetNativeOcclusionEnabled(std::get<std::uint32_t>(target), bEnabled, pResource);
}

bool CLuaOcclusionDefs::IsOcclusionEnabled(std::variant<CClientOcclusion*, std::uint32_t> target)
{
    if (std::holds_alternative<CClientOcclusion*>(target))
        return CStaticFunctionDefinitions::IsOcclusionEnabled(std::get<CClientOcclusion*>(target));

    return CStaticFunctionDefinitions::IsNativeOcclusionEnabled(std::get<std::uint32_t>(target));
}

int CLuaOcclusionDefs::GetOcclusions(lua_State* luaVM)
{
    std::vector<bool> tables;
    if (lua_isnoneornil(luaVM, 1))
    {
        tables.push_back(false);
        tables.push_back(true);
    }
    else
        tables.push_back(lua_toboolean(luaVM, 1) != 0);

    lua_newtable(luaVM);
    int iIndex = 1;

    for (bool bInterior : tables)
    {
        std::vector<SOcclusionZoneEntry> entries;
        CStaticFunctionDefinitions::GetOcclusions(bInterior, entries);

        for (const SOcclusionZoneEntry& entry : entries)
        {
            lua_newtable(luaVM);

            lua_pushnumber(luaVM, static_cast<double>(entry.id));
            lua_setfield(luaVM, -2, "id");
            lua_pushboolean(luaVM, entry.bInterior);
            lua_setfield(luaVM, -2, "interior");

            lua_pushnumber(luaVM, entry.vecPosition.fX);
            lua_setfield(luaVM, -2, "posX");
            lua_pushnumber(luaVM, entry.vecPosition.fY);
            lua_setfield(luaVM, -2, "posY");
            lua_pushnumber(luaVM, entry.vecPosition.fZ);
            lua_setfield(luaVM, -2, "posZ");

            lua_pushnumber(luaVM, entry.vecSize.fX);
            lua_setfield(luaVM, -2, "sizeX");
            lua_pushnumber(luaVM, entry.vecSize.fY);
            lua_setfield(luaVM, -2, "sizeY");
            lua_pushnumber(luaVM, entry.vecSize.fZ);
            lua_setfield(luaVM, -2, "sizeZ");

            lua_pushnumber(luaVM, entry.vecRotation.fX);
            lua_setfield(luaVM, -2, "rotX");
            lua_pushnumber(luaVM, entry.vecRotation.fY);
            lua_setfield(luaVM, -2, "rotY");
            lua_pushnumber(luaVM, entry.vecRotation.fZ);
            lua_setfield(luaVM, -2, "rotZ");

            lua_pushboolean(luaVM, entry.bEnabled);
            lua_setfield(luaVM, -2, "enabled");

            lua_rawseti(luaVM, -2, iIndex++);
        }
    }

    return 1;
}
