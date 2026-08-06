/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientOcclusionManager.cpp
 *  PURPOSE:     Occlusion zone manager class
 *
 *****************************************************************************/

#include "StdInc.h"

CClientOcclusionManager::CClientOcclusionManager(CClientManager* pManager) : m_pManager(pManager), m_bPopulated(false)
{
}

CClientOcclusionManager::~CClientOcclusionManager()
{
    for (CClientOcclusion* pOcclusion : m_MapZones)
        if (pOcclusion)
            delete pOcclusion;

    for (CClientOcclusion* pOcclusion : m_InteriorZones)
        if (pOcclusion)
            delete pOcclusion;
}

void CClientOcclusionManager::PopulateFromNative()
{
    if (m_bPopulated)
        return;
    m_bPopulated = true;

    COcclusion*    pOcclusion = g_pGame->GetOcclusion();
    CClientEntity* pRoot = g_pClientGame->GetRootEntity();

    m_MapZones.reserve(pOcclusion->GetZoneCount(false));
    for (std::size_t i = 0; i < pOcclusion->GetZoneCount(false); i++)
    {
        CClientOcclusion* pZone = new CClientOcclusion(m_pManager, INVALID_ELEMENT_ID, i, false, true);
        pZone->SetParent(pRoot);
        m_MapZones.push_back(pZone);
    }

    m_InteriorZones.reserve(pOcclusion->GetZoneCount(true));
    for (std::size_t i = 0; i < pOcclusion->GetZoneCount(true); i++)
    {
        CClientOcclusion* pZone = new CClientOcclusion(m_pManager, INVALID_ELEMENT_ID, i, true, true);
        pZone->SetParent(pRoot);
        m_InteriorZones.push_back(pZone);
    }
}

CClientOcclusion* CClientOcclusionManager::CreateCustomZone(const CVector& vecPosition, const CVector& vecSize, const CVector& vecRotation, bool bInterior)
{
    std::vector<CClientOcclusion*>& list = bInterior ? m_InteriorZones : m_MapZones;

    SOcclusionZoneInfo info;
    info.vecPosition = vecPosition;
    info.vecSize = vecSize;
    info.vecRotation = vecRotation;

    std::size_t index;
    if (!g_pGame->GetOcclusion()->CreateZone(info, bInterior, index))
        return nullptr;

    if (index != list.size())
        return nullptr;  // Something else touched the native table out of band; bail rather than misalign indices

    CClientOcclusion* pZone = new CClientOcclusion(m_pManager, INVALID_ELEMENT_ID, index, bInterior, false);
    list.push_back(pZone);
    return pZone;
}

void CClientOcclusionManager::RemoveFromList(CClientOcclusion* pOcclusion)
{
    std::vector<CClientOcclusion*>& list = pOcclusion->IsInterior() ? m_InteriorZones : m_MapZones;
    if (pOcclusion->GetIndex() < list.size())
        list[pOcclusion->GetIndex()] = nullptr;
}
