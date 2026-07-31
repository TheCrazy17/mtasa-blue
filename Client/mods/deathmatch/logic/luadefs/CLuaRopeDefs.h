/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/luadefs/CLuaRopeDefs.h
 *  PURPOSE:     Lua rope class functions
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once
#include "CLuaDefs.h"

class CLuaRopeDefs : public CLuaDefs
{
public:
    static void LoadFunctions();
    static void AddClass(lua_State* luaVM);

    LUA_DECLARE(CreateRope);
    LUA_DECLARE(DestroyRope);
    LUA_DECLARE(AttachRopeToEntity);
    LUA_DECLARE(DetachRopeEntity);
    LUA_DECLARE(GetRopeAttachedElement);
    LUA_DECLARE(IsElementAttachedToRope);
    LUA_DECLARE(GetRopeType);
    LUA_DECLARE(SetRopeSegmentLength);
    LUA_DECLARE(GetRopeSegmentLength);
    LUA_DECLARE(SetRopeAnchorVelocity);
    LUA_DECLARE(GetRopeHookPosition);
    LUA_DECLARE(GetRopeSegmentPosition);
};
