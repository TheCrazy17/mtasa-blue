/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientStreamSector.cpp
 *  PURPOSE:     Stream sector class
 *
 *****************************************************************************/

#include "StdInc.h"

using std::list;

CClientStreamSector::CClientStreamSector(CClientStreamSectorRow* pRow, CVector2D& vecBottomLeft, CVector2D& vecTopRight)
{
    m_pRow = pRow;
    m_vecBottomLeft = vecBottomLeft;
    m_vecTopRight = vecTopRight;
    m_pLeft = nullptr;
    m_pRight = nullptr;
    m_pTop = nullptr;
    m_pBottom = nullptr;
    m_bActivated = false;
    m_bExtra = false;

    m_pArea = nullptr;
}

CClientStreamSector::~CClientStreamSector()
{
    // Remove our connected sectors
    if (m_pLeft)
        m_pLeft->m_pRight = nullptr;
    if (m_pRight)
        m_pRight->m_pLeft = nullptr;
    if (m_pTop)
        m_pTop->m_pBottom = nullptr;
    if (m_pBottom)
        m_pBottom->m_pTop = nullptr;
}

bool CClientStreamSector::DoesContain(CVector& vecPosition)
{
    if (vecPosition.fX >= m_vecBottomLeft.fX && vecPosition.fX < m_vecTopRight.fX)
    {
        if (vecPosition.fY >= m_vecBottomLeft.fY && vecPosition.fY < m_vecTopRight.fY)
        {
            return true;
        }
    }
    return false;
}

bool CClientStreamSector::DoesContain(float fX)
{
    if (fX >= m_vecBottomLeft.fX && fX < m_vecTopRight.fX)
    {
        return true;
    }
    return false;
}

void CClientStreamSector::GetSurroundingSectors(CClientStreamSector** pArray)
{
    memset(pArray, 0, sizeof(pArray) * 8);
    pArray[1] = m_pTop;
    pArray[3] = m_pLeft;
    pArray[4] = m_pRight;
    pArray[6] = m_pBottom;
    if (m_pTop)
    {
        pArray[0] = m_pTop->m_pLeft;
        pArray[2] = m_pTop->m_pRight;
    }
    else
    {
        if (m_pLeft)
            pArray[0] = m_pLeft->m_pTop;
        if (m_pRight)
            pArray[2] = m_pRight->m_pTop;
    }
    if (m_pBottom)
    {
        pArray[5] = m_pBottom->m_pLeft;
        pArray[7] = m_pBottom->m_pRight;
    }
    else
    {
        if (m_pLeft)
            pArray[5] = m_pLeft->m_pBottom;
        if (m_pRight)
            pArray[7] = m_pRight->m_pBottom;
    }
}

bool CClientStreamSector::IsMySurroundingSector(CClientStreamSector* pSector)
{
    CClientStreamSector* pSurrounding[8];
    GetSurroundingSectors(pSurrounding);
    for (int i = 0; i < 8; i++)
    {
        if (pSurrounding[i] && pSurrounding[i] == pSector)
        {
            return true;
        }
    }
    return false;
}

void CClientStreamSector::CompareSurroundings(CClientStreamSector* pSector, list<CClientStreamSector*>* pCommon, list<CClientStreamSector*>* pUncommon,
                                              bool bIncludeCenter)
{
    // Make sure our lists are cleared
    pCommon->clear();
    pUncommon->clear();

    CClientStreamSector* pSurrounding[8];
    pSector->GetSurroundingSectors(pSurrounding);
    for (int i = 0; i < 8; i++)
    {
        if (pSurrounding[i])
        {
            if (IsMySurroundingSector(pSurrounding[i]))
                pCommon->push_back(pSurrounding[i]);
            else
                pUncommon->push_back(pSurrounding[i]);
        }
    }

    if (bIncludeCenter)
    {
        if (IsMySurroundingSector(pSector))
            pCommon->push_back(pSector);
        else
            pUncommon->push_back(pSector);
    }
}

void CClientStreamSector::AddElements(list<CClientStreamElement*>* pList, std::unordered_set<CClientStreamElement*>* pSet)
{
    list<CClientStreamElement*>::iterator iter = m_Elements.begin();
    for (; iter != m_Elements.end(); iter++)
    {
        CClientStreamElement* pElement = *iter;

        // Don't add if already in the list (O(1) if set provided)
        if (pSet)
        {
            if (pSet->count(pElement))
                continue;
            pSet->insert(pElement);
        }
        else if (ListContains(*pList, pElement))
            continue;

        pElement->SetActiveElementIter(pList->insert(pList->end(), pElement));
    }
}

void CClientStreamSector::RemoveElements(list<CClientStreamElement*>* pList, std::unordered_set<CClientStreamElement*>* pSet)
{
    list<CClientStreamElement*>::iterator iter = m_Elements.begin();
    for (; iter != m_Elements.end(); iter++)
    {
        CClientStreamElement* pElement = *iter;

        // Erase by iterator instead of by value so this stays O(1) per element
        if (pSet)
        {
            if (pSet->erase(pElement))
                pList->erase(pElement->GetActiveElementIter());
        }
        else
        {
            pList->remove(pElement);
        }
    }
}
