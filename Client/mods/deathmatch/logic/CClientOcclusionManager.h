/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientOcclusionManager.h
 *  PURPOSE:     Occlusion zone manager class
 *
 *****************************************************************************/

#pragma once

#include <vector>
#include "CClientOcclusion.h"

class CClientOcclusionManager
{
    friend class CClientOcclusion;

public:
    CClientOcclusionManager(class CClientManager* pManager);
    ~CClientOcclusionManager();

    CClientOcclusion* CreateCustomZone(const CVector& vecPosition, const CVector& vecSize, const CVector& vecRotation, bool bInterior);

    const std::vector<CClientOcclusion*>& GetMapZones() const noexcept { return m_MapZones; }
    const std::vector<CClientOcclusion*>& GetInteriorZones() const noexcept { return m_InteriorZones; }

private:
    void RemoveFromList(CClientOcclusion* pOcclusion);

    class CClientManager*          m_pManager;
    std::vector<CClientOcclusion*> m_MapZones;
    std::vector<CClientOcclusion*> m_InteriorZones;
};
