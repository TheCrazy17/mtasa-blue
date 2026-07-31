/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/luadefs/CLuaRopeDefs.cpp
 *  PURPOSE:     Lua rope class functions
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <game/CRopes.h>
#include "CClientRope.h"

namespace
{
    // Matches eRopeTypeSA in Client/game_sa/CRopeInstanceSA.h
    int RopeTypeFromName(const SString& strName)
    {
        if (strName == "craneMagnet")
            return 1;
        if (strName == "craneHarness")
            return 2;
        if (strName == "magnet")
            return 3;
        if (strName == "craneMagno")
            return 4;
        if (strName == "wreckingBall")
            return 5;
        if (strName == "quarryCraneArm")
            return 6;
        if (strName == "craneTrolley")
            return 7;
        if (strName == "swat")
            return 8;
        return 0;            // NONE
    }

    const char* RopeTypeToName(int iType)
    {
        switch (iType)
        {
            case 1:
                return "craneMagnet";
            case 2:
                return "craneHarness";
            case 3:
                return "magnet";
            case 4:
                return "craneMagno";
            case 5:
                return "wreckingBall";
            case 6:
                return "quarryCraneArm";
            case 7:
                return "craneTrolley";
            case 8:
                return "swat";
            default:
                return "none";
        }
    }
}            // namespace

void CLuaRopeDefs::LoadFunctions()
{
    constexpr static const std::pair<const char*, lua_CFunction> functions[]{
        {"createRope", CreateRope},
        {"destroyRope", DestroyRope},
        {"attachRopeToEntity", AttachRopeToEntity},
        {"detachRopeEntity", DetachRopeEntity},
        {"getRopeAttachedElement", GetRopeAttachedElement},
        {"isElementAttachedToRope", IsElementAttachedToRope},
        {"getRopeType", GetRopeType},
        {"setRopeSegmentLength", SetRopeSegmentLength},
        {"getRopeSegmentLength", GetRopeSegmentLength},
        {"setRopeAnchorVelocity", SetRopeAnchorVelocity},
        {"getRopeHookPosition", GetRopeHookPosition},
        {"getRopeSegmentPosition", GetRopeSegmentPosition},
    };

    for (const auto& [name, func] : functions)
        CLuaCFunctions::AddFunction(name, func);
}

void CLuaRopeDefs::AddClass(lua_State* luaVM)
{
    lua_newclass(luaVM);

    lua_classfunction(luaVM, "create", "createRope");
    lua_classfunction(luaVM, "destroy", "destroyRope");
    lua_classfunction(luaVM, "attachEntity", "attachRopeToEntity");
    lua_classfunction(luaVM, "detachEntity", "detachRopeEntity");
    lua_classfunction(luaVM, "getAttachedElement", "getRopeAttachedElement");
    lua_classfunction(luaVM, "getType", "getRopeType");
    lua_classfunction(luaVM, "setSegmentLength", "setRopeSegmentLength");
    lua_classfunction(luaVM, "getSegmentLength", "getRopeSegmentLength");
    lua_classfunction(luaVM, "setAnchorVelocity", "setRopeAnchorVelocity");
    lua_classfunction(luaVM, "getHookPosition", "getRopeHookPosition");
    lua_classfunction(luaVM, "getSegmentPosition", "getRopeSegmentPosition");

    lua_registerclass(luaVM, "Rope", "Element");
}

int CLuaRopeDefs::CreateRope(lua_State* luaVM)
{
    //  rope createRope ( float x, float y, float z, string ropeType [, element holder ] )
    CVector         vecPosition;
    SString         strRopeType;
    CClientEntity*  pHolder = nullptr;

    CScriptArgReader argStream(luaVM);
    argStream.ReadVector3D(vecPosition);
    argStream.ReadString(strRopeType);
    argStream.ReadUserData(pHolder, nullptr);

    if (!argStream.HasErrors())
    {
        int iRopeType = RopeTypeFromName(strRopeType);
        if (iRopeType == 0)
        {
            m_pScriptDebugging->LogCustom(luaVM, SString("Unknown rope type '%s'", *strRopeType));
        }
        else
        {
            CLuaMain*  pLuaMain = m_pLuaManager->GetVirtualMachine(luaVM);
            CResource* pResource = pLuaMain ? pLuaMain->GetResource() : nullptr;

            if (pResource)
            {
                auto pRope = new CClientRope(m_pManager, INVALID_ELEMENT_ID, iRopeType, vecPosition, pHolder);
                if (pRope->IsValid())
                {
                    pRope->SetParent(pResource->GetResourceDynamicEntity());

                    CElementGroup* pGroup = pResource->GetElementGroup();
                    if (pGroup)
                        pGroup->Add(pRope);

                    lua_pushelement(luaVM, pRope);
                    return 1;
                }
                delete pRope;
                m_pScriptDebugging->LogCustom(luaVM, "createRope failed (native rope pool exhausted?)");
            }
        }
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::DestroyRope(lua_State* luaVM)
{
    //  bool destroyRope ( rope theRope )
    CClientRope* pRope;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);

    if (!argStream.HasErrors())
    {
        delete pRope;
        lua_pushboolean(luaVM, true);
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::AttachRopeToEntity(lua_State* luaVM)
{
    //  bool attachRopeToEntity ( rope theRope, element theEntity )
    CClientRope*   pRope;
    CClientEntity* pEntity;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);
    argStream.ReadUserData(pEntity);

    if (!argStream.HasErrors())
    {
        lua_pushboolean(luaVM, pRope->AttachEntity(pEntity));
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::DetachRopeEntity(lua_State* luaVM)
{
    //  bool detachRopeEntity ( rope theRope )
    CClientRope* pRope;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);

    if (!argStream.HasErrors())
    {
        pRope->DetachEntity();
        lua_pushboolean(luaVM, true);
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::GetRopeAttachedElement(lua_State* luaVM)
{
    //  element getRopeAttachedElement ( rope theRope )
    CClientRope* pRope;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);

    if (!argStream.HasErrors())
    {
        CClientEntity* pAttached = pRope->GetAttachedEntity();
        if (pAttached)
        {
            lua_pushelement(luaVM, pAttached);
            return 1;
        }
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::IsElementAttachedToRope(lua_State* luaVM)
{
    //  bool isElementAttachedToRope ( element theEntity )
    CClientEntity* pEntity;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pEntity);

    if (!argStream.HasErrors())
    {
        // Walk every rope element instead of the game_sa pool directly, so this only ever
        // considers ropes actually owned by a script (not any stray native ropes).
        bool bAttached = pEntity->GetGameEntity() && g_pGame->GetRopes()->IsEntityAttachedToRope(pEntity->GetGameEntity()->GetInterface());
        lua_pushboolean(luaVM, bAttached);
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::GetRopeType(lua_State* luaVM)
{
    //  string getRopeType ( rope theRope )
    CClientRope* pRope;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);

    if (!argStream.HasErrors())
    {
        lua_pushstring(luaVM, RopeTypeToName(pRope->GetRopeType()));
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::SetRopeSegmentLength(lua_State* luaVM)
{
    //  bool setRopeSegmentLength ( rope theRope, float length )
    CClientRope* pRope;
    float        fLength;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);
    argStream.ReadNumber(fLength);

    if (!argStream.HasErrors())
    {
        lua_pushboolean(luaVM, pRope->SetSegmentLength(fLength));
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::GetRopeSegmentLength(lua_State* luaVM)
{
    //  float getRopeSegmentLength ( rope theRope )
    CClientRope* pRope;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);

    if (!argStream.HasErrors())
    {
        lua_pushnumber(luaVM, pRope->GetSegmentLength());
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::SetRopeAnchorVelocity(lua_State* luaVM)
{
    //  bool setRopeAnchorVelocity ( rope theRope, float x, float y, float z )
    CClientRope* pRope;
    CVector      vecVelocity;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);
    argStream.ReadVector3D(vecVelocity);

    if (!argStream.HasErrors())
    {
        lua_pushboolean(luaVM, pRope->SetAnchorVelocity(vecVelocity));
        return 1;
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::GetRopeHookPosition(lua_State* luaVM)
{
    //  float, float, float getRopeHookPosition ( rope theRope )
    // Live position of the hook/magnet prop, not the fixed anchor point getElementPosition returns.
    // The rope's segment simulation doesn't collide with world geometry, so a rope with no real holder
    // can swing or fall through the ground - use this to keep track of it and detach/destroy yourself
    // once it's somewhere you don't want it (e.g. below ground).
    CClientRope* pRope;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);

    if (!argStream.HasErrors())
    {
        CVector vecPosition;
        if (pRope->GetHookPosition(vecPosition))
        {
            lua_pushnumber(luaVM, vecPosition.fX);
            lua_pushnumber(luaVM, vecPosition.fY);
            lua_pushnumber(luaVM, vecPosition.fZ);
            return 3;
        }
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}

int CLuaRopeDefs::GetRopeSegmentPosition(lua_State* luaVM)
{
    //  float, float, float getRopeSegmentPosition ( rope theRope, int index )
    // Raw per-node chain positions (index 0..31 - CRope::Render() always draws exactly 32 nodes,
    // regardless of the rope's type or how many are actively simulated). The native render draws these
    // as a single 1px line, same as vanilla crane cables - there's no native way to make that thicker.
    // Use this to draw your own representation on top (e.g. dxDrawLine3D between consecutive nodes) if
    // you need one.
    CClientRope* pRope;
    int          iIndex;

    CScriptArgReader argStream(luaVM);
    argStream.ReadUserData(pRope);
    argStream.ReadNumber(iIndex);

    if (!argStream.HasErrors())
    {
        CVector vecPosition;
        if (iIndex >= 0 && pRope->GetSegmentPosition(static_cast<unsigned char>(iIndex), vecPosition))
        {
            lua_pushnumber(luaVM, vecPosition.fX);
            lua_pushnumber(luaVM, vecPosition.fY);
            lua_pushnumber(luaVM, vecPosition.fZ);
            return 3;
        }
    }
    else
        m_pScriptDebugging->LogCustom(luaVM, argStream.GetFullErrorMessage());

    lua_pushboolean(luaVM, false);
    return 1;
}
