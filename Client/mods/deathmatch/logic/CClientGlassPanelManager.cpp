/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientGlassPanelManager.cpp
 *  PURPOSE:     Glass panel entity manager class
 *
 *****************************************************************************/

#include <StdInc.h>

CClientGlassPanelManager::CClientGlassPanelManager(CClientManager* pManager)
{
    m_pManager = pManager;
    m_bDontRemoveFromList = false;
}

CClientGlassPanelManager::~CClientGlassPanelManager()
{
    DeleteAll();
}

CClientGlassPanel* CClientGlassPanelManager::Create(ElementID ID)
{
    if (Get(ID))
        return nullptr;

    return new CClientGlassPanel(m_pManager, ID);
}

void CClientGlassPanelManager::DeleteAll()
{
    m_bDontRemoveFromList = true;
    for (CClientGlassPanel* pPanel : m_List)
        delete pPanel;
    m_bDontRemoveFromList = false;

    m_List.clear();
}

CClientGlassPanel* CClientGlassPanelManager::Get(ElementID ID)
{
    CClientEntity* pEntity = CElementIDs::GetElement(ID);
    if (pEntity && pEntity->GetType() == CCLIENTGLASSPANEL)
        return static_cast<CClientGlassPanel*>(pEntity);

    return nullptr;
}

void CClientGlassPanelManager::DoPulse()
{
    for (CClientGlassPanel* pPanel : m_List)
        pPanel->DoPulse();
}

void CClientGlassPanelManager::RemoveFromList(CClientGlassPanel* pPanel)
{
    if (!m_bDontRemoveFromList && !m_List.empty())
        m_List.remove(pPanel);
}
