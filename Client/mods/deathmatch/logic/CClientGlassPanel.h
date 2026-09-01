/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientGlassPanel.h
 *  PURPOSE:     Glass panel entity class header
 *
 *****************************************************************************/

#pragma once

#include "CClientEntity.h"

class CClientGlassPanelManager;
class CClientObject;

// A flat, colourable panel drawn with the client's own primitive renderer instead of a real GTA
// model, so it never inherits the alpha sort/z-write glitches native transparent models have. See
// CClientGlassPanel.cpp for how breaking one reuses the native CGlass shard pool.
class CClientGlassPanel final : public CClientEntity
{
    DECLARE_CLASS(CClientGlassPanel, CClientEntity)
    friend class CClientGlassPanelManager;

public:
    CClientGlassPanel(CClientManager* pManager, ElementID ID);
    ~CClientGlassPanel();

    void Unlink();

    eClientEntityType GetType() const { return CCLIENTGLASSPANEL; };

    void GetPosition(CVector& vecPosition) const { vecPosition = m_vecPosition; };
    void SetPosition(const CVector& vecPosition);

    void GetRotationRadians(CVector& vecOutRadians) const { vecOutRadians = m_vecRotation; };
    void SetRotationRadians(const CVector& vecRadians);

    void GetSize(float& fWidth, float& fHeight) const
    {
        fWidth = m_fWidth;
        fHeight = m_fHeight;
    };
    void SetSize(float fWidth, float fHeight);

    float GetThickness() const { return m_fThickness; };
    void  SetThickness(float fThickness) { m_fThickness = fThickness; };

    const SColor& GetColor() const { return m_Color; };
    void          SetColor(const SColor& color) { m_Color = color; };

    bool IsBreakable() const { return m_bBreakable; };
    void SetBreakable(bool bBreakable) { m_bBreakable = bBreakable; };

    bool IsBroken() const { return m_bBroken; };
    bool Break(const CVector& vecForce, unsigned char ucGranularity);

    bool IsCollisionEnabled() const { return m_bCollisionEnabled; };
    bool SetCollisionEnabled(bool bEnabled);

protected:
    void DoPulse();

private:
    void UpdateCollision();

    CClientManager*           m_pManager;
    CClientGlassPanelManager* m_pGlassPanelManager;

    CVector m_vecPosition;
    CVector m_vecRotation;
    float   m_fWidth;
    float   m_fHeight;
    float   m_fThickness;
    SColor  m_Color;
    bool    m_bBreakable;
    bool    m_bBroken;

    bool           m_bCollisionEnabled;
    CClientObject* m_pCollisionObject;
};
