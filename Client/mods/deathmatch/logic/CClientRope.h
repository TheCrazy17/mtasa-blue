/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CClientRope.h
 *  PURPOSE:     Rope entity class header
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "CClientEntity.h"

class CClientRopeManager;

class CClientRope final : public CClientEntity
{
    DECLARE_CLASS(CClientRope, CClientEntity)
    friend class CClientRopeManager;

public:
    CClientRope(CClientManager* pManager, ElementID ID, int iRopeType, const CVector& vecPosition, CClientEntity* pHolder = nullptr);
    ~CClientRope();

    void              Unlink();
    eClientEntityType GetType() const { return CCLIENTROPE; }

    // Position is fixed at creation - the rope itself is native-simulated from there, moving it
    // isn't meaningful the way moving a normal element is.
    void GetPosition(CVector& vecPosition) const { vecPosition = m_vecPosition; }
    void SetPosition(const CVector& vecPosition) {}

    bool IsValid() const { return m_iRopeHandle != -1; }

    bool           AttachEntity(CClientEntity* pEntity);
    void           DetachEntity();
    CClientEntity* GetAttachedEntity() const;

    int   GetRopeType() const;
    bool  SetSegmentLength(float length);
    float GetSegmentLength() const;
    bool  SetAnchorVelocity(const CVector& velocity);
    bool  GetHookPosition(CVector& outPosition) const;
    bool  GetSegmentPosition(unsigned char index, CVector& outPosition) const;

private:
    CClientManager* m_pManager;
    int             m_iRopeHandle;
    CVector         m_vecPosition;
};
