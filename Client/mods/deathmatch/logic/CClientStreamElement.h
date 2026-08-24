/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientStreamElement.h
 *  PURPOSE:     Streamed entity class header
 *
 *****************************************************************************/

#pragma once

#include "CClientEntity.h"
#include <cstdint>
#include <list>
class CClientStreamer;
class CClientStreamSector;
class CClientStreamSectorRow;

class CClientStreamElement : public CClientEntity
{
    DECLARE_CLASS(CClientStreamElement, CClientEntity)
    friend class CClientStreamer;
    friend class CClientStreamSector;

public:
    CClientStreamElement(CClientStreamer* pStreamer, ElementID ID);
    ~CClientStreamElement();

    void                                       UpdateStreamPosition(const CVector& vecPosition);
    CVector                                    GetStreamPosition() { return m_vecStreamPosition; }
    CClientStreamSectorRow*                    GetStreamRow() { return m_pStreamRow; }
    CClientStreamSector*                       GetStreamSector() { return m_pStreamSector; }
    std::list<CClientStreamElement*>::iterator GetSectorElementIter() { return m_SectorElementIter; }
    std::list<CClientStreamElement*>::iterator GetActiveElementIter() { return m_ActiveElementIter; }
    bool                                       IsStreamedIn() { return m_bStreamedIn; }
    void                                       InternalStreamIn(bool bInstantly);
    void                                       InternalStreamOut();
    virtual void                               StreamIn(bool bInstantly) = 0;
    virtual void                               StreamOut() = 0;
    virtual void                               NotifyCreate();
    void                                       NotifyUnableToCreate();
    void                                       AddStreamReference(bool bScript = false);
    void                                       RemoveStreamReference(bool bScript = false);
    unsigned short                             GetStreamReferences(bool bScript = false);
    unsigned long                              GetTotalStreamReferences() { return m_usStreamReferences + m_usStreamReferencesScript; }
    std::uint32_t                              GetLastStreamOutTime() const { return m_lastStreamOutTime; }
    void                                       StreamOutForABit();
    void                                       SetDimension(unsigned short usDimension) override;
    float                                      GetExpDistance() { return m_fExpDistance; }
    virtual CSphere                            GetWorldBoundingSphere();
    float                                      GetDistanceToBoundingBoxSquared(const CVector& vecPosition);

    bool IsStreamingCompatibleClass() { return true; };

    virtual bool IsVisibleInAllDimensions() { return false; };

    // Whether this element currently belongs to the streamer's own dimension. Not const,
    // since GetDimension() and IsVisibleInAllDimensions() aren't const anywhere in this
    // hierarchy, and not noexcept, since it dereferences the streamer pointer.
    bool IsInStreamerDimension() { return GetDimension() == m_pStreamer->GetDimension() || IsVisibleInAllDimensions(); }

    // Lets a subclass force a dimension admission recheck even when SetDimension's own
    // no-op guard skipped calling OnElementDimension (see CClientObject::SetVisibleInAllDimensions)
    void NotifyDimensionRecheck(unsigned short usOldDimension) { m_pStreamer->OnElementDimension(this, usOldDimension); }

private:
    void SetStreamRow(CClientStreamSectorRow* pRow) { m_pStreamRow = pRow; }
    void SetStreamSector(CClientStreamSector* pSector) { m_pStreamSector = pSector; }
    void SetExpDistance(float fDistance) { m_fExpDistance = fDistance; }
    void SetSectorElementIter(std::list<CClientStreamElement*>::iterator iter) { m_SectorElementIter = iter; }
    void SetActiveElementIter(std::list<CClientStreamElement*>::iterator iter) { m_ActiveElementIter = iter; }

    CClientStreamSectorRow*                    m_pStreamRow;
    CClientStreamSector*                       m_pStreamSector;
    std::list<CClientStreamElement*>::iterator m_SectorElementIter;
    std::list<CClientStreamElement*>::iterator m_ActiveElementIter;
    CVector                                    m_vecStreamPosition;
    float                                      m_fExpDistance;
    unsigned short                             m_usStreamReferences, m_usStreamReferencesScript;

protected:
    CClientStreamer* m_pStreamer;
    bool             m_bStreamedIn;
    bool             m_bAttemptingToStreamIn;
    std::uint32_t    m_lastStreamOutTime;

public:
    float                   m_fCachedRadius;
    int                     m_iCachedRadiusCounter;
    SFixedArray<CVector, 2> m_vecCachedBoundingBox;
    int                     m_iCachedBoundingBoxCounter;
};
