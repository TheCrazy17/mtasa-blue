/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CClientRope.cpp
 *  PURPOSE:     Rope entity class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <game/CRopes.h>
#include <game/CPools.h>
#include "CClientRope.h"
#include "CClientRopeManager.h"

CClientRope::CClientRope(CClientManager* pManager, ElementID ID, int iRopeType, const CVector& vecPosition, CClientEntity* pHolder)
    : ClassInit(this), CClientEntity(ID)
{
    SetTypeName("rope");

    m_pManager = pManager;
    m_vecPosition = vecPosition;
    m_iRopeHandle = -1;

    CEntitySAInterface* pHolderInterface = nullptr;
    if (pHolder && pHolder->GetGameEntity())
        pHolderInterface = pHolder->GetGameEntity()->GetInterface();

    m_iRopeHandle = g_pGame->GetRopes()->CreateRope(iRopeType, vecPosition, pHolderInterface);

    pManager->GetRopeManager()->AddToList(this);
}

CClientRope::~CClientRope()
{
    Unlink();
}

void CClientRope::Unlink()
{
    if (m_iRopeHandle != -1)
    {
        g_pGame->GetRopes()->DestroyRope(m_iRopeHandle);
        m_iRopeHandle = -1;
    }
    m_pManager->GetRopeManager()->RemoveFromList(this);
}

bool CClientRope::AttachEntity(CClientEntity* pEntity)
{
    if (m_iRopeHandle == -1 || !pEntity || !pEntity->GetGameEntity())
        return false;

    return g_pGame->GetRopes()->AttachRopeToEntity(m_iRopeHandle, pEntity->GetGameEntity()->GetInterface());
}

void CClientRope::DetachEntity()
{
    if (m_iRopeHandle != -1)
        g_pGame->GetRopes()->DetachRopeEntity(m_iRopeHandle);
}

CClientEntity* CClientRope::GetAttachedEntity() const
{
    if (m_iRopeHandle == -1)
        return nullptr;

    CEntitySAInterface* pInterface = g_pGame->GetRopes()->GetRopeAttachedEntity(m_iRopeHandle);
    if (!pInterface)
        return nullptr;

    return g_pGame->GetPools()->GetClientEntity((DWORD*)pInterface);
}

int CClientRope::GetRopeType() const
{
    return m_iRopeHandle != -1 ? g_pGame->GetRopes()->GetRopeType(m_iRopeHandle) : -1;
}

bool CClientRope::SetSegmentLength(float length)
{
    return m_iRopeHandle != -1 ? g_pGame->GetRopes()->SetRopeSegmentLength(m_iRopeHandle, length) : false;
}

float CClientRope::GetSegmentLength() const
{
    return m_iRopeHandle != -1 ? g_pGame->GetRopes()->GetRopeSegmentLength(m_iRopeHandle) : 0.0f;
}

bool CClientRope::SetAnchorVelocity(const CVector& velocity)
{
    return m_iRopeHandle != -1 ? g_pGame->GetRopes()->SetRopeAnchorVelocity(m_iRopeHandle, velocity) : false;
}

bool CClientRope::GetHookPosition(CVector& outPosition) const
{
    return m_iRopeHandle != -1 ? g_pGame->GetRopes()->GetRopeHookPosition(m_iRopeHandle, outPosition) : false;
}

bool CClientRope::GetSegmentPosition(unsigned char index, CVector& outPosition) const
{
    return m_iRopeHandle != -1 ? g_pGame->GetRopes()->GetRopeSegmentPosition(m_iRopeHandle, index, outPosition) : false;
}
