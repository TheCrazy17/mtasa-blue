/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientStreamer.cpp
 *  PURPOSE:     Streamer class
 *
 *****************************************************************************/

#include "StdInc.h"
#include <algorithm>
#include <cstdint>
using std::list;

void* CClientStreamer::pAddingElement = NULL;

CClientStreamer::CClientStreamer(StreamerLimitReachedFunction* pLimitReachedFunc, float fMaxDistance, float fSectorSize, float fRowSize)
    : m_fSectorSize(fSectorSize), m_fRowSize(fRowSize)
{
    // Setup our distance variables
    m_fMaxDistanceExp = fMaxDistance * fMaxDistance;
    m_fMaxDistanceThreshold = (fMaxDistance + 50.0f) * (fMaxDistance + 50.0f);
    m_usDimension = 0;

    // We need the limit reached func
    assert(pLimitReachedFunc);
    m_pLimitReachedFunc = pLimitReachedFunc;

    // Create our main world sectors covering the mainland
    CVector2D size(m_fSectorSize, m_fRowSize);
    CVector2D bottomLeft(-WORLD_SIZE, -WORLD_SIZE);
    CVector2D topRight(WORLD_SIZE, WORLD_SIZE);
    CreateSectors(&m_WorldRows, size, bottomLeft, topRight);

    // Find our row and sector
    m_pRow = FindOrCreateRow(m_vecPosition);
    m_pSector = NULL;
    OnEnterSector(m_pRow->FindOrCreateSector(m_vecPosition));
}

CClientStreamer::~CClientStreamer()
{
    // Clear our mainland rows
    list<CClientStreamSectorRow*>::iterator iter = m_WorldRows.begin();
    for (; iter != m_WorldRows.end(); iter++)
    {
        delete *iter;
    }
    m_WorldRows.clear();

    // Clear our extra rows
    for (auto& [key, pRow] : m_ExtraRows)
    {
        delete pRow;
    }
    m_ExtraRows.clear();
}

void CClientStreamer::CreateSectors(std::list<CClientStreamSectorRow*>* pList, CVector2D& vecSize, CVector2D& vecBottomLeft, CVector2D& vecTopRight)
{
    // Creates our sectors within rows, filling up our rectangle, connecting each sector and row
    CClientStreamSector *   pCurrent = NULL, *pPrevious = NULL, *pPreviousRowSector = NULL;
    CClientStreamSectorRow *pCurrentRow = NULL, *pPreviousRow = NULL;
    float                   fX = vecBottomLeft.fX, fY = vecBottomLeft.fY;

    while (fY < vecTopRight.fY)
    {
        pCurrentRow = new CClientStreamSectorRow(fY, fY + vecSize.fY, m_fSectorSize, m_fRowSize);
        pCurrentRow->m_pBottom = pPreviousRow;
        pList->push_back(pCurrentRow);

        if (pPreviousRow)
            pPreviousRow->m_pTop = pCurrentRow;

        while (fX < vecTopRight.fX)
        {
            CVector2D bottomLeft(fX, fY);
            CVector2D topRight(fX + vecSize.fX, fY + vecSize.fY);

            pPrevious = pCurrent;
            pCurrent = new CClientStreamSector(pCurrentRow, bottomLeft, topRight);
            pCurrentRow->Add(pCurrent);
            pCurrent->m_pLeft = pPrevious;

            if (pPrevious)
                pPrevious->m_pRight = pCurrent;

            if (pPreviousRowSector)
            {
                pCurrent->m_pBottom = pPreviousRowSector;
                pPreviousRowSector->m_pTop = pCurrent;
                pPreviousRowSector = pPreviousRowSector->m_pRight;
            }

            fX += vecSize.fX;
        }

        pPrevious = NULL;
        pCurrent = NULL;
        pPreviousRow = pCurrentRow;
        pPreviousRowSector = pPreviousRow->Front();
        fX = vecBottomLeft.fX;
        fY += vecSize.fY;
    }
}

void CClientStreamer::ConnectRow(CClientStreamSectorRow* pRow)
{
    float fTop, fBottom;
    pRow->GetPosition(fTop, fBottom);

    // Connect up our row
    pRow->m_pTop = FindRow(fTop + (m_fRowSize / 2.0f));
    pRow->m_pBottom = FindRow(fBottom - (m_fRowSize / 2.0f));

    // Connect the other rows to us
    if (pRow->m_pTop)
        pRow->m_pTop->m_pBottom = pRow;
    if (pRow->m_pBottom)
        pRow->m_pBottom->m_pTop = pRow;
}

#include "..\deathmatch\logic\CClientGame.h"
extern CClientGame* g_pClientGame;
void                CClientStreamer::DoPulse(CVector& vecPosition)
{
    /* Debug code
    CClientStreamSector * pSector;
    list < CClientStreamSector * > ::iterator iterSector;
    list < CClientStreamSectorRow * > ::iterator iterRow = m_WorldRows.begin ();
    for ( ; iterRow != m_WorldRows.end () ; iterRow++ )
    {
        iterSector = (*iterRow)->Begin ();
        for ( ; iterSector != (*iterRow)->End () ; iterSector++ )
        {
            pSector = *iterSector;
            if ( !pSector->m_pArea )
            {
                pSector->m_pArea = new CClientRadarArea ( g_pClientGame->GetManager (), INVALID_ELEMENT_ID );
                pSector->m_pArea->SetPosition ( pSector->m_vecBottomLeft );
                CVector2D vecSize ( pSector->m_vecTopRight.fX - pSector->m_vecBottomLeft.fX, pSector->m_vecTopRight.fY - pSector->m_vecBottomLeft.fY );
                pSector->m_pArea->SetSize ( vecSize );
                pSector->m_pArea->SetColor ( 255, 0, 0, 50 );
            }
            pSector->m_pArea->SetColor ( 255, 0, 0, 50 );
        }
    }
    iterRow = m_ExtraRows.begin ();
    for ( ; iterRow != m_ExtraRows.end () ; iterRow++ )
    {
        iterSector = (*iterRow)->Begin ();
        for ( ; iterSector != (*iterRow)->End () ; iterSector++ )
        {
            pSector = *iterSector;
            if ( !pSector->m_pArea )
            {
                pSector->m_pArea = new CClientRadarArea ( g_pClientGame->GetManager (), INVALID_ELEMENT_ID );
                pSector->m_pArea->SetPosition ( pSector->m_vecBottomLeft );
                CVector2D vecSize ( pSector->m_vecTopRight.fX - pSector->m_vecBottomLeft.fX, pSector->m_vecTopRight.fY - pSector->m_vecBottomLeft.fY );
                pSector->m_pArea->SetSize ( vecSize );
                pSector->m_pArea->SetColor ( 255, 0, 0, 50 );
            }
            pSector->m_pArea->SetColor ( 255, 0, 0, 50 );
        }
    }
    */

    bool bMovedFar = false;
    // Has our position changed?
    if (vecPosition != m_vecPosition)
    {
        bMovedFar = ((m_vecPosition - vecPosition).LengthSquared() > (50 * 50));
        m_vecPosition = vecPosition;

        // Have we changed row?
        if (!m_pRow->DoesContain(vecPosition))
        {
            m_pRow = FindOrCreateRow(vecPosition, m_pRow);
            OnEnterSector(m_pRow->FindOrCreateSector(vecPosition));
        }
        // Have we changed sector?
        else if (!m_pSector->DoesContain(vecPosition))
        {
            // Grab our new sector
            OnEnterSector(m_pRow->FindOrCreateSector(vecPosition, m_pSector));
        }
    }

    // Update distances every frame
    SetExpDistances(&m_ActiveElements);
    m_ActiveElements.sort(CompareExpDistance);

    Restream(bMovedFar);
}

void CClientStreamer::SetDimension(unsigned short usDimension)
{
    if (usDimension == m_usDimension)
        return;

    m_usDimension = usDimension;

    auto evictIfWrongDimension = [this](CClientStreamElement* pElement)
    {
        if (!pElement->IsInStreamerDimension())
            DemoteElement(pElement);
    };

    // Every element currently in the sector grid belongs to the old dimension, so it all has
    // to be re-checked. Snapshot each sector's elements before mutating it, since DemoteElement
    // erases from the very list being walked here (via OnElementEnterSector).
    for (CClientStreamSectorRow* pRow : m_WorldRows)
    {
        for (auto sectorIter = pRow->Begin(); sectorIter != pRow->End(); ++sectorIter)
        {
            std::vector<CClientStreamElement*> snapshot((*sectorIter)->Begin(), (*sectorIter)->End());
            std::ranges::for_each(snapshot, evictIfWrongDimension);
        }
    }
    for (auto& [rowIndex, pRow] : m_ExtraRows)
    {
        for (auto sectorIter = pRow->Begin(); sectorIter != pRow->End(); ++sectorIter)
        {
            std::vector<CClientStreamElement*> snapshot((*sectorIter)->Begin(), (*sectorIter)->End());
            std::ranges::for_each(snapshot, evictIfWrongDimension);
        }
    }

    // Only the bucket for our new dimension can have anything to promote
    auto bucketIter = m_OutOfDimensionElements.find(m_usDimension);
    if (bucketIter != m_OutOfDimensionElements.end())
    {
        std::vector<CClientStreamElement*> toPromote(bucketIter->second.begin(), bucketIter->second.end());
        for (CClientStreamElement* pElement : toPromote)
        {
            bucketIter->second.erase(pElement);
            AdmitElement(pElement);
        }
    }
}

CClientStreamSectorRow* CClientStreamer::FindOrCreateRow(CVector& vecPosition, CClientStreamSectorRow* pSurrounding)
{
    // Do we have a row to check around first?
    if (pSurrounding)
    {
        // Check the above and below rows
        if (pSurrounding->m_pTop && pSurrounding->m_pTop->DoesContain(vecPosition))
            return pSurrounding->m_pTop;
        if (pSurrounding->m_pBottom && pSurrounding->m_pBottom->DoesContain(vecPosition))
            return pSurrounding->m_pBottom;
    }

    // Search through our main world rows
    CClientStreamSectorRow*                 pRow = NULL;
    list<CClientStreamSectorRow*>::iterator iter = m_WorldRows.begin();
    for (; iter != m_WorldRows.end(); iter++)
    {
        pRow = *iter;
        if (pRow->DoesContain(vecPosition))
        {
            return pRow;
        }
    }

    // Search through our extra rows using map lookup
    float fBottom = float((int)(vecPosition.fY / m_fRowSize)) * m_fRowSize;
    if (vecPosition.fY < 0.0f)
        fBottom -= m_fRowSize;
    int iRowIndex = (int)(fBottom / m_fRowSize);

    auto it = m_ExtraRows.find(iRowIndex);
    if (it != m_ExtraRows.end())
        return it->second;

    // We need a new row, align it with the others
    pRow = new CClientStreamSectorRow(fBottom, fBottom + m_fRowSize, m_fSectorSize, m_fRowSize);
    ConnectRow(pRow);
    pRow->SetExtra(true);
    m_ExtraRows[iRowIndex] = pRow;
    return pRow;
}

CClientStreamSectorRow* CClientStreamer::FindRow(float fY)
{
    // Search through our main world rows
    CClientStreamSectorRow*                 pRow = NULL;
    list<CClientStreamSectorRow*>::iterator iter = m_WorldRows.begin();
    for (; iter != m_WorldRows.end(); iter++)
    {
        pRow = *iter;
        if (pRow->DoesContain(fY))
        {
            return pRow;
        }
    }

    // Search through our extra rows using map lookup
    float fBottom = float((int)(fY / m_fRowSize)) * m_fRowSize;
    if (fY < 0.0f)
        fBottom -= m_fRowSize;
    int iRowIndex = (int)(fBottom / m_fRowSize);

    auto it = m_ExtraRows.find(iRowIndex);
    if (it != m_ExtraRows.end())
        return it->second;

    return NULL;
}

void CClientStreamer::OnUpdateStreamPosition(CClientStreamElement* pElement)
{
    if (!pElement->GetStreamRow())
        return;  // parked outside our dimension, nothing to keep in sync

    CVector                 vecPosition = pElement->GetStreamPosition();
    CClientStreamSectorRow* pRow = pElement->GetStreamRow();
    CClientStreamSector*    pSector = pElement->GetStreamSector();

    // Have we changed row?
    if (!pRow->DoesContain(vecPosition))
    {
        pRow = FindOrCreateRow(vecPosition);
        pElement->SetStreamRow(pRow);
        OnElementEnterSector(pElement, pRow->FindOrCreateSector(vecPosition));
    }
    // Have we changed sector?
    else if (!pSector->DoesContain(vecPosition))
    {
        // Grab our new sector
        OnElementEnterSector(pElement, pRow->FindOrCreateSector(vecPosition, pSector));
    }
    else
    {
        // Make sure our distance is updated
        pElement->SetExpDistance(pElement->GetDistanceToBoundingBoxSquared(m_vecPosition));
    }
}

void CClientStreamer::AdmitElement(CClientStreamElement* pElement)
{
    CVector                 vecPosition = pElement->GetStreamPosition();
    CClientStreamSectorRow* pRow = FindOrCreateRow(vecPosition);
    pElement->SetStreamRow(pRow);
    OnElementEnterSector(pElement, pRow->FindOrCreateSector(vecPosition));
}

void CClientStreamer::DemoteElement(CClientStreamElement* pElement)
{
    if (pElement->IsStreamedIn())
        m_ToStreamOut.push_back(pElement);

    pElement->SetStreamRow(nullptr);
    OnElementEnterSector(pElement, nullptr);  // detaches sector membership only

    if (m_ActiveElementSet.erase(pElement))
        m_ActiveElements.erase(pElement->GetActiveElementIter());

    m_OutOfDimensionElements[pElement->GetDimension()].insert(pElement);
}

void CClientStreamer::AddElement(CClientStreamElement* pElement)
{
    assert(pAddingElement == NULL);  // pre-existing static guard, unrelated to dimension admission
    pAddingElement = pElement;

    if (pElement->IsInStreamerDimension())
        AdmitElement(pElement);
    else
        m_OutOfDimensionElements[pElement->GetDimension()].insert(pElement);

    pAddingElement = NULL;
}

void CClientStreamer::RemoveElement(CClientStreamElement* pElement)
{
    OnElementEnterSector(pElement, NULL);
    if (m_ActiveElementSet.erase(pElement))
        m_ActiveElements.erase(pElement->GetActiveElementIter());
    m_OutOfDimensionElements[pElement->GetDimension()].erase(pElement);  // no-op if never parked
    m_ToStreamOut.remove(pElement);
}

void CClientStreamer::SetExpDistances(list<CClientStreamElement*>* pList)
{
    // Run through our list setting distances to world center
    CClientStreamElement*                 pElement = NULL;
    list<CClientStreamElement*>::iterator iter = pList->begin();
    for (; iter != pList->end(); iter++)
    {
        pElement = *iter;
        // Set its distance ^ 2
        pElement->SetExpDistance(pElement->GetDistanceToBoundingBoxSquared(m_vecPosition));
    }
}

void CClientStreamer::AddToSortedList(list<CClientStreamElement*>* pList, CClientStreamElement* pElement)
{
    // Make sure it's exp distance is updated
    float fDistance = pElement->GetDistanceToBoundingBoxSquared(m_vecPosition);
    pElement->SetExpDistance(fDistance);

    // Don't add if already in the list (O(1) check)
    if (m_ActiveElementSet.count(pElement))
        return;

    // Track in the set
    m_ActiveElementSet.insert(pElement);

    // Append unsorted - DoPulse sorts the list every frame via m_ActiveElements.sort()
    pElement->SetActiveElementIter(pList->insert(pList->end(), pElement));
}

bool CClientStreamer::CompareExpDistance(CClientStreamElement* p1, CClientStreamElement* p2)
{
    return p1->GetExpDistance() < p2->GetExpDistance();
}

bool CClientStreamer::IsActiveElement(CClientStreamElement* pElement)
{
    return m_ActiveElementSet.count(pElement) > 0;
}

void CClientStreamer::Restream(bool bMovedFar)
{
    // Avoid swap ping-pong when two candidates are almost the same distance.
    // Distances are squared, so compare against squared hysteresis too.
    constexpr float         swapHysteresisDistanceSq = 10.0f * 10.0f;
    constexpr std::uint32_t minStreamInDelayAfterOutMs = 100u;
    const std::uint32_t     currentTime = static_cast<std::uint32_t>(CClientTime::GetTime());

    // Limit distance stream in/out rate
    // Vehicles might have to ignore this to reduce blocking loads elsewhere.
    int iMaxOut = 6;
    int iMaxIn = 6;
    if (bMovedFar)
    {
        iMaxOut = 1000;
        iMaxIn = 1000;
    }

    // Do we have any elements waiting to be streamed out?
    while (!m_ToStreamOut.empty())
    {
        CClientStreamElement* pElement = m_ToStreamOut.front();
        // Make sure we have no stream-references
        if (pElement->GetTotalStreamReferences() == 0)
        {
            // Stream out 1 of them per frame
            pElement->InternalStreamOut();
            iMaxOut--;
        }
        m_ToStreamOut.remove(pElement);

        if (iMaxOut <= 0)
            break;
    }

    static std::vector<CClientStreamElement*> ClosestStreamedOutList;
    static std::vector<CClientStreamElement*> FurthestStreamedInList;
    ClosestStreamedOutList.clear();
    FurthestStreamedInList.clear();

    bool bReachedLimit = ReachedLimit();
    // Loop through our active elements list (they should be ordered closest to furthest)
    list<CClientStreamElement*>::iterator iter = m_ActiveElements.begin();
    for (; iter != m_ActiveElements.end(); iter++)
    {
        CClientStreamElement* pElement = *iter;
        float                 fElementDistanceExp = pElement->GetExpDistance();

        // Is this element streamed in?
        if (pElement->IsStreamedIn())
        {
            if (IS_VEHICLE(pElement))
            {
                CClientVehicle* pVehicle = DynamicCast<CClientVehicle>(pElement);
                if (pVehicle)
                {
                    if (pVehicle->GetOccupant() && IS_PLAYER(pVehicle->GetOccupant()))
                    {
                        CClientPlayer* pPlayer = DynamicCast<CClientPlayer>(pVehicle->GetOccupant());
                        if (pPlayer->GetLastPuresyncType() == PURESYNC_TYPE_LIGHTSYNC)
                        {
                            // if the last packet was ls he shouldn't be streamed in
                            m_ToStreamOut.push_back(pElement);
                        }
                    }

                    // Is this a trailer?
                    if (pVehicle->GetTowedByVehicle() != NULL)
                    {
                        // Don't stream it out (this is handled by the towing vehicle)
                        continue;
                    }
                }
            }
            if (IS_PLAYER(pElement))
            {
                CClientPlayer* pPlayer = DynamicCast<CClientPlayer>(pElement);
                if (pPlayer->GetLastPuresyncType() == PURESYNC_TYPE_LIGHTSYNC)
                {
                    // if the last packet was ls he isn't/shouldn't be streamed in
                    m_ToStreamOut.push_back(pElement);
                }
            }
            // Too far away? Use the threshold so we won't flicker load it if it's on the border moving.
            if (fElementDistanceExp > m_fMaxDistanceThreshold)
            {
                // Unstream it now?
                if (iMaxOut > 0)
                {
                    // Make sure we have no stream-references
                    if (pElement->GetTotalStreamReferences() == 0)
                    {
                        // Stream out now
                        pElement->InternalStreamOut();
                        iMaxOut--;
                    }
                    m_ToStreamOut.remove(pElement);
                }
                else
                {
                    // or later
                    m_ToStreamOut.push_back(pElement);
                }
            }
            else
            {
                FurthestStreamedInList.push_back(pElement);
            }
        }
        else
        {
            // AddElement/OnElementDimension/SetDimension all funnel demotion through
            // DemoteElement, so an off-dimension element should never reach this list; this
            // is a defense in depth check, not the primary gate.
            if (!pElement->IsInStreamerDimension())
            {
                assert(false && "off-dimension element leaked into m_ActiveElements, demotion invariant violated");
                continue;
            }

            // Too far away? Stop here.
            if (fElementDistanceExp > m_fMaxDistanceExp)
                continue;

            if (IS_VEHICLE(pElement))
            {
                CClientVehicle* pVehicle = DynamicCast<CClientVehicle>(pElement);
                if (pVehicle && pVehicle->GetOccupant() && IS_PLAYER(pVehicle->GetOccupant()))
                {
                    CClientPlayer* pPlayer = DynamicCast<CClientPlayer>(pVehicle->GetOccupant());
                    if (pPlayer->GetLastPuresyncType() == PURESYNC_TYPE_LIGHTSYNC)
                    {
                        // if the last packet was ls he isn't streaming in soon.
                        continue;
                    }
                }

                if (pVehicle && pVehicle->GetTowedByVehicle())
                {
                    // Streaming in of towed vehicles is done in CClientVehicle::StreamIn by the towing vehicle
                    continue;
                }
            }
            if (IS_PLAYER(pElement))
            {
                CClientPlayer* pPlayer = DynamicCast<CClientPlayer>(pElement);
                if (pPlayer->GetLastPuresyncType() == PURESYNC_TYPE_LIGHTSYNC)
                {
                    // if the last packet was ls he isn't streaming in soon.
                    continue;
                }
            }
            // If attached and attached-to is streamed out, don't consider for streaming in
            CClientStreamElement* pAttachedTo = DynamicCast<CClientStreamElement>(pElement->GetAttachedTo());
            if (pAttachedTo && !pAttachedTo->IsStreamedIn())
            {
                // ...unless attached to low LOD version
                CClientObject* pAttachedToObject = DynamicCast<CClientObject>(pAttachedTo);
                CClientObject* pObject = DynamicCast<CClientObject>(pElement);
                if (!pObject || !pAttachedToObject || pObject->IsLowLod() == pAttachedToObject->IsLowLod())
                    continue;
            }

            // Prevent rapid in/out thrashing of the same element.
            if (!bMovedFar && (currentTime - pElement->GetLastStreamOutTime()) < minStreamInDelayAfterOutMs)
                continue;

            // Not room to stream in more elements?
            if (bReachedLimit)
            {
                // Add to the list that might be streamed in during the final phase
                if ((int)ClosestStreamedOutList.size() < iMaxIn)  // (only add if there is a chance it will be used)
                    ClosestStreamedOutList.push_back(pElement);
            }
            else
            {
                // Stream in the new element. Don't do it instantly unless moved from far away.
                pElement->InternalStreamIn(bMovedFar);
                bReachedLimit = ReachedLimit();

                if (!bReachedLimit)
                {
                    iMaxIn--;
                    if (iMaxIn <= 0)
                        break;
                }
            }
        }
    }

    // Complex code of doom:
    //      ClosestStreamedOutList is {nearest to furthest} list of streamed out elements within streaming distance
    //      FurthestStreamedInList is {nearest to furthest} list of streamed in elements
    if (bReachedLimit)
    {
        // Check 'furthest streamed in' against 'closest streamed out' to see if the state can be swapped
        int  iFurthestStreamedInIndex = FurthestStreamedInList.size() - 1;
        uint uiClosestStreamedOutIndex = 0;
        for (uint i = 0; i < 10; i++)
        {
            // Check limits for this frame
            if (iMaxIn <= 0 || iMaxOut <= 0)
                break;

            // Check indices are valid
            if (iFurthestStreamedInIndex < 0)
                break;
            if (uiClosestStreamedOutIndex >= ClosestStreamedOutList.size())
                break;

            // See if ClosestStreamedOut is nearer than FurthestStreamedIn
            CClientStreamElement* pFurthestStreamedIn = FurthestStreamedInList[iFurthestStreamedInIndex];
            CClientStreamElement* pClosestStreamedOut = ClosestStreamedOutList[uiClosestStreamedOutIndex];
            if ((pClosestStreamedOut->GetExpDistance() + swapHysteresisDistanceSq) >= pFurthestStreamedIn->GetExpDistance())
                break;

            // Stream out FurthestStreamedIn candidate if possible
            if (pFurthestStreamedIn->GetTotalStreamReferences() == 0)
            {
                // Stream out now
                pFurthestStreamedIn->InternalStreamOut();
                iMaxOut--;
            }
            m_ToStreamOut.remove(pFurthestStreamedIn);
            iFurthestStreamedInIndex--;  // Always advance to the next candidate

            // Stream in ClosestStreamedOut candidate if possible
            if (!ReachedLimit())
            {
                if (!bMovedFar && (currentTime - pClosestStreamedOut->GetLastStreamOutTime()) < minStreamInDelayAfterOutMs)
                    continue;

                // Stream in the new element. No need to do it instantly unless moved from far away.
                pClosestStreamedOut->InternalStreamIn(bMovedFar);
                iMaxIn--;
                uiClosestStreamedOutIndex++;
            }
        }
    }
}

void CClientStreamer::OnEnterSector(CClientStreamSector* pSector)
{
    CClientStreamElement* pElement = NULL;
    if (m_pSector)
    {
        // Grab the unwanted sectors
        list<CClientStreamSector*> common, uncommon;
        pSector->CompareSurroundings(m_pSector, &common, &uncommon, true);

        // Deactivate the unwanted sectors
        CClientStreamSector*                 pTempSector = NULL;
        list<CClientStreamSector*>::iterator iter = uncommon.begin();
        for (; iter != uncommon.end(); iter++)
        {
            pTempSector = *iter;
            // Make sure we dont unload our new sector
            if (pTempSector != pSector)
            {
                if (pTempSector->IsActivated())
                {
                    list<CClientStreamElement*>::iterator iter = pTempSector->Begin();
                    for (; iter != pTempSector->End(); iter++)
                    {
                        pElement = *iter;
                        if (pElement->IsStreamedIn())
                        {
                            // Add it to our streaming out list
                            m_ToStreamOut.push_back(pElement);
                        }
                    }
                    pTempSector->RemoveElements(&m_ActiveElements, &m_ActiveElementSet);
                    pTempSector->SetActivated(false);
                }
            }
        }

        // Grab the wanted sectors
        m_pSector->CompareSurroundings(pSector, &common, &uncommon, true);

        // Activate the unwanted sectors
        iter = uncommon.begin();
        for (; iter != uncommon.end(); iter++)
        {
            pTempSector = *iter;
            if (!pTempSector->IsActivated())
            {
                pTempSector->AddElements(&m_ActiveElements, &m_ActiveElementSet);
                pTempSector->SetActivated(true);
            }
        }
    }
    m_pSector = pSector;
}

void CClientStreamer::OnElementEnterSector(CClientStreamElement* pElement, CClientStreamSector* pSector)
{
    CClientStreamSector* pPreviousSector = pElement->GetStreamSector();
    if (pPreviousSector)
    {
        // Skip if disconnecting
        if (g_pClientGame->IsBeingDeleted())
            return;

        // Remove the element from its old sector
        pPreviousSector->Remove(pElement->GetSectorElementIter());
    }
    if (pSector)
    {
        // Add the element to its new sector
        pElement->SetSectorElementIter(pSector->Add(pElement));

        // Is this new sector activated?
        if (pSector->IsActivated())
        {
            // Was the previous sector not active?
            if (!pPreviousSector || !pPreviousSector->IsActivated())
            {
                // Add this element to our active-elements list
                AddToSortedList(&m_ActiveElements, pElement);
            }
        }
        else
        {
            // Should we activate this sector?
            if (pSector->IsExtra() && (m_pSector->IsMySurroundingSector(pSector) || m_pSector == pSector))
            {
                pSector->AddElements(&m_ActiveElements, &m_ActiveElementSet);
                pSector->SetActivated(true);
            }
            // If we're in a deactivated sector and streamed in, stream us out
            else if (pElement->IsStreamedIn())
            {
                m_ToStreamOut.push_back(pElement);
            }
        }
    }
    pElement->SetStreamSector(pSector);
}

void CClientStreamer::OnElementForceStreamIn(CClientStreamElement* pElement)
{
    // Make sure we're streamed in
    pElement->InternalStreamIn(true);
}

void CClientStreamer::OnElementForceStreamOut(CClientStreamElement* pElement)
{
    // Make sure we're streamed out if need be
    if (!IsActiveElement(pElement))
    {
        m_ToStreamOut.push_back(pElement);
    }
}

void CClientStreamer::OnElementDimension(CClientStreamElement* pElement, unsigned short usOldDimension)
{
    bool bWasAdmitted = (pElement->GetStreamSector() != nullptr);
    bool bShouldBeAdmitted = pElement->IsInStreamerDimension();

    if (bWasAdmitted && !bShouldBeAdmitted)
    {
        DemoteElement(pElement);
    }
    else if (!bWasAdmitted && bShouldBeAdmitted)
    {
        m_OutOfDimensionElements[usOldDimension].erase(pElement);
        AdmitElement(pElement);
    }
    else if (!bWasAdmitted && !bShouldBeAdmitted && usOldDimension != pElement->GetDimension())
    {
        // Still outside, but the numeric dimension changed from one foreign one to another;
        // relocate buckets so a later promotion still finds it
        m_OutOfDimensionElements[usOldDimension].erase(pElement);
        m_OutOfDimensionElements[pElement->GetDimension()].insert(pElement);
    }
    // else already correctly admitted, or already correctly parked; nothing to do
}
