/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/CRopes.h
 *  PURPOSE:     Rope entity interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

class CVector;
class CEntitySAInterface;

class CRopes
{
public:
    virtual int  CreateRopeForSwatPed(const CVector& vecPosition, DWORD dwDuration = 4000) = 0;
    virtual void RemoveEntityRope(CEntitySAInterface* pObjectSA) = 0;

    // Own rope pool, not limited to the native 8 slots. Ropes created here live outside the
    // native ms_aRopes array entirely - see plans/effervescent-prancing-pebble.md.
    // ropeType matches eRopeTypeSA in CRopeInstanceSA.h (1-8).
    virtual int  CreateRope(int ropeType, const CVector& vecPosition, CEntitySAInterface* pHolder = nullptr) = 0;
    virtual void DestroyRope(int ropeHandle) = 0;
    virtual void UpdateRopes() = 0;            // call once per logic frame
    virtual void RenderRopes() = 0;            // call once per render frame (hooked into the game's own rope render pass)

    virtual bool                AttachRopeToEntity(int ropeHandle, CEntitySAInterface* pEntity) = 0;
    virtual void                DetachRopeEntity(int ropeHandle) = 0;
    virtual CEntitySAInterface* GetRopeAttachedEntity(int ropeHandle) = 0;
    virtual bool                IsEntityAttachedToRope(CEntitySAInterface* pEntity) = 0;

    virtual int   GetRopeType(int ropeHandle) = 0;                     // -1 if handle invalid
    virtual bool  SetRopeSegmentLength(int ropeHandle, float length) = 0;
    virtual float GetRopeSegmentLength(int ropeHandle) = 0;            // 0.0f if handle invalid
    virtual bool  SetRopeAnchorVelocity(int ropeHandle, const CVector& speed) = 0;
    virtual bool  GetRopeHookPosition(int ropeHandle, CVector& outPosition) = 0;            // live position of the hook/magnet prop
    virtual bool  GetRopeSegmentPosition(int ropeHandle, uint8 index, CVector& outPosition) = 0;            // index 0..31
};
