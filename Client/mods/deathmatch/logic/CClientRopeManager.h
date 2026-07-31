/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CClientRopeManager.h
 *  PURPOSE:     Rope entity manager class header
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <list>

class CClientRope;

class CClientRopeManager
{
public:
    void AddToList(CClientRope* pRope) { m_Ropes.push_back(pRope); }
    void RemoveFromList(CClientRope* pRope) { m_Ropes.remove(pRope); }

    // Native ropes update/render themselves once CRopesSA::PulseRopes() is called each frame
    // (see CClientGame::DoPulses) - this manager doesn't need to iterate them itself for that,
    // just tracks which elements exist for lookup/cleanup.
    unsigned int Count() const { return static_cast<unsigned int>(m_Ropes.size()); }

private:
    std::list<CClientRope*> m_Ropes;
};
