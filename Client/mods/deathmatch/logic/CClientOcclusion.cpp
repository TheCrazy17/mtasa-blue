/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientOcclusion.cpp
 *  PURPOSE:     Occlusion zone entity class
 *
 *****************************************************************************/

#include "StdInc.h"

CClientOcclusion::CClientOcclusion(CClientManager* pManager, ElementID ID, std::size_t index, bool bInterior, bool bNative)
    : ClassInit(this),
      CClientEntity(ID),
      m_pOcclusionManager(pManager->GetOcclusionManager()),
      m_index(index),
      m_bInterior(bInterior),
      m_bNative(bNative),
      m_bEnabled(true)
{
    m_pManager = pManager;
    SetTypeName("occlusion");

    // Native zones exist for the whole session; destroyElement() must not be able to drop the
    // element and orphan the still-live native slot. setOcclusionEnabled is the only way to
    // touch those. Custom zones keep the default (destroyable).
    if (m_bNative)
        SetCanBeDestroyedByScript(false);

    SOcclusionZoneInfo info;
    if (g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        m_vecLastEnabledSize = info.vecSize;
}

CClientOcclusion::~CClientOcclusion()
{
    // Custom zones can't be removed from the native table, only shrunk into irrelevance;
    // native zones are left as-is, only setOcclusionEnabled ever touches those.
    if (!m_bNative)
        SetEnabled(false);

    m_pOcclusionManager->RemoveFromList(this);
}

void CClientOcclusion::GetPosition(CVector& vecPosition) const
{
    SOcclusionZoneInfo info;
    if (g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        vecPosition = info.vecPosition;
}

void CClientOcclusion::SetPosition(const CVector& vecPosition)
{
    SOcclusionZoneInfo info;
    if (!g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        return;

    info.vecPosition = vecPosition;
    g_pGame->GetOcclusion()->SetZoneData(m_index, m_bInterior, info);
}

bool CClientOcclusion::GetSize(CVector& vecSize) const
{
    SOcclusionZoneInfo info;
    if (!g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        return false;

    vecSize = m_bEnabled ? info.vecSize : m_vecLastEnabledSize;
    return true;
}

bool CClientOcclusion::SetSize(const CVector& vecSize)
{
    if (!m_bEnabled)
    {
        // Remember it for when it's re-enabled instead of writing it to a disabled zone
        m_vecLastEnabledSize = vecSize;
        return true;
    }

    SOcclusionZoneInfo info;
    if (!g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        return false;

    info.vecSize = vecSize;
    m_vecLastEnabledSize = vecSize;
    return g_pGame->GetOcclusion()->SetZoneData(m_index, m_bInterior, info);
}

bool CClientOcclusion::GetRotation(CVector& vecRotation) const
{
    SOcclusionZoneInfo info;
    if (!g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        return false;

    vecRotation = info.vecRotation;
    return true;
}

bool CClientOcclusion::SetRotation(const CVector& vecRotation)
{
    SOcclusionZoneInfo info;
    if (!g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        return false;

    info.vecRotation = vecRotation;
    return g_pGame->GetOcclusion()->SetZoneData(m_index, m_bInterior, info);
}

bool CClientOcclusion::SetEnabled(bool bEnabled)
{
    if (bEnabled == m_bEnabled)
        return true;

    SOcclusionZoneInfo info;
    if (!g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        return false;

    if (bEnabled)
    {
        info.vecSize = m_vecLastEnabledSize;
    }
    else
    {
        m_vecLastEnabledSize = info.vecSize;
        info.vecSize = CVector(0.01f, 0.01f, 0.01f);  // shrink rather than remove; see CreateZone/AddOne
    }

    if (!g_pGame->GetOcclusion()->SetZoneData(m_index, m_bInterior, info))
        return false;

    m_bEnabled = bEnabled;
    return true;
}

//
// Draw a translucent, cross-braced box matching the zone's actual world position/size/rotation
//
void CClientOcclusion::DebugRender(const CVector& vecPosition, float fDrawRadius)
{
    SOcclusionZoneInfo info;
    if (!g_pGame->GetOcclusion()->GetZoneData(m_index, m_bInterior, info))
        return;

    CVector vecSize = m_bEnabled ? info.vecSize : m_vecLastEnabledSize;

    // Zones can be building-sized, so cull against the camera distance to the nearest point
    // of the box, not its center; otherwise a large zone the camera is standing inside of
    // (but whose center is far away) gets skipped even though it's actively occluding.
    const float fZoneRadius = vecSize.Length() * 0.5f;
    if ((info.vecPosition - vecPosition).Length() > fDrawRadius + fZoneRadius)
        return;

    // AddOne bakes a +180 degree offset into the stored rotation before encoding it (see
    // COcclusion::AddOne in the reversed engine); GetZoneData already strips that offset back
    // out so scripts see the value they'd pass in. For matching the engine's actual box
    // orientation here, that offset has to go back in.
    CVector vecRawRotationRadians = info.vecRotation + CVector(180.0f, 180.0f, 180.0f);
    ConvertDegreesToRadians(vecRawRotationRadians);

    const float fSinX = sinf(vecRawRotationRadians.fX), fCosX = cosf(vecRawRotationRadians.fX);
    const float fSinY = sinf(vecRawRotationRadians.fY), fCosY = cosf(vecRawRotationRadians.fY);
    const float fSinZ = sinf(vecRawRotationRadians.fZ), fCosZ = cosf(vecRawRotationRadians.fZ);

    // Matches the native engine's CMatrix::SetRotate(x, y, z) exactly (game_sa Core/Matrix.cpp);
    // that's what COccluder::ProcessOneOccluder uses to build the real occluder box, and MTA's
    // own CMatrix uses a different convention that draws the box in the wrong orientation for
    // anything but a zero rotation.
    const CVector axisRight(fCosZ * fCosY - fSinZ * fSinX * fSinY, fCosZ * fSinX * fSinY + fSinZ * fCosY, -(fSinY * fCosX));
    const CVector axisForward(-(fSinZ * fCosX), fCosZ * fCosX, fSinX);
    const CVector axisUp(fCosZ * fSinY + fSinZ * fSinX * fCosY, fSinZ * fSinY - fCosZ * fSinX * fCosY, fCosY * fCosX);

    // vecSize.fX/fY/fZ are width/length/height respectively (see SetZoneData); those map to the
    // right/forward/up axes the same way COccluder::ProcessOneOccluder builds its box.
    const CVector halfWidth = axisRight * (vecSize.fX * 0.5f);
    const CVector halfLength = axisForward * (vecSize.fY * 0.5f);
    const CVector halfHeight = axisUp * (vecSize.fZ * 0.5f);

    const CVector& center = info.vecPosition;
    CVector        worldCorners[8] = {
        center - halfWidth - halfLength - halfHeight, center + halfWidth - halfLength - halfHeight, center + halfWidth + halfLength - halfHeight,
        center - halfWidth + halfLength - halfHeight, center - halfWidth - halfLength + halfHeight, center + halfWidth - halfLength + halfHeight,
        center + halfWidth + halfLength + halfHeight, center - halfWidth + halfLength + halfHeight,
    };

    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},  // bottom face
        {4, 5}, {5, 6}, {6, 7}, {7, 4},  // top face
        {0, 4}, {1, 5}, {2, 6}, {3, 7},  // vertical edges
    };

    // Each face as a quad, corners listed in perimeter order; used both for the diagonal
    // cross braces and the translucent fill triangles.
    static const int faces[6][4] = {
        {0, 1, 2, 3},  // bottom
        {4, 5, 6, 7},  // top
        {0, 1, 5, 4},  // back
        {3, 2, 6, 7},  // front
        {0, 3, 7, 4},  // left
        {1, 2, 6, 5},  // right
    };

    // Colored by kind so map/interior/custom zones are distinguishable at a glance; dimmed
    // (not recolored) when disabled so the kind still reads even when it's shrunk down.
    // The pre-existing global toggle (setOcclusionsEnabled) overrides all of that: if the whole
    // system is off, nothing here is actually occluding, so it's shown as flat grey regardless
    // of kind or per-zone state, rather than looking like it's still working.
    const bool bGloballyEnabled = g_pGame->GetWorld()->GetOcclusionsEnabled();

    SColorARGB baseColor = !m_bNative ? SColorARGB(255, 60, 220, 90) :  // custom: green
                               m_bInterior ? SColorARGB(255, 60, 180, 255)
                                           :                  // native, interior table: blue
                               SColorARGB(255, 255, 140, 0);  // native, map table: orange
    if (!bGloballyEnabled)
        baseColor = SColorARGB(255, 120, 120, 120);

    const SColorARGB colorEdge(m_bEnabled && bGloballyEnabled ? 220 : 90, baseColor.R, baseColor.G, baseColor.B);
    const SColorARGB colorFill(m_bEnabled && bGloballyEnabled ? 50 : 20, baseColor.R, baseColor.G, baseColor.B);

    CGraphicsInterface* const pGraphics = g_pCore->GetGraphics();

    for (const auto& edge : edges)
        pGraphics->DrawLine3DQueued(worldCorners[edge[0]], worldCorners[edge[1]], 3.0f, colorEdge, eRenderStage::POST_FX);

    for (const auto& face : faces)
    {
        pGraphics->DrawLine3DQueued(worldCorners[face[0]], worldCorners[face[2]], 1.0f, colorEdge, eRenderStage::POST_FX);
        pGraphics->DrawLine3DQueued(worldCorners[face[1]], worldCorners[face[3]], 1.0f, colorEdge, eRenderStage::POST_FX);
    }

    auto* pVertices = new std::vector<PrimitiveVertice>();
    pVertices->reserve(6 * 6);
    for (const auto& face : faces)
    {
        const CVector faceCorners[4] = {worldCorners[face[0]], worldCorners[face[1]], worldCorners[face[2]], worldCorners[face[3]]};
        const int     triangleIndices[6] = {0, 1, 2, 0, 2, 3};
        for (int idx : triangleIndices)
            pVertices->push_back({faceCorners[idx].fX, faceCorners[idx].fY, faceCorners[idx].fZ, colorFill});
    }
    pGraphics->DrawPrimitive3DQueued(pVertices, D3DPT_TRIANGLELIST, eRenderStage::POST_FX);
}
