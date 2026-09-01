/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientGlassPanelManager.h
 *  PURPOSE:     Glass panel entity manager class header
 *
 *****************************************************************************/

#pragma once

#include <list>
#include "CClientGlassPanel.h"

class CClientManager;

class CClientGlassPanelManager
{
    friend class CClientGlassPanel;

public:
    CClientGlassPanelManager(CClientManager* pManager);
    ~CClientGlassPanelManager();

    CClientGlassPanel* Create(ElementID ID);
    void               DeleteAll();
    CClientGlassPanel* Get(ElementID ID);

    void DoPulse();

    unsigned int Count() { return static_cast<unsigned int>(m_List.size()); };

protected:
    void AddToList(CClientGlassPanel* pPanel) { m_List.push_back(pPanel); };
    void RemoveFromList(CClientGlassPanel* pPanel);

private:
    CClientManager*               m_pManager;
    std::list<CClientGlassPanel*> m_List;
    bool                          m_bDontRemoveFromList;
};
