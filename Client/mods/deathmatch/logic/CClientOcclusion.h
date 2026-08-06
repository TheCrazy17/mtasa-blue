/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientOcclusion.h
 *  PURPOSE:     Occlusion zone entity class
 *
 *****************************************************************************/

#pragma once

#include <game/COcclusion.h>

class CClientOcclusionManager;

class CClientOcclusion final : public CClientEntity
{
    DECLARE_CLASS(CClientOcclusion, CClientEntity)
public:
    CClientOcclusion(CClientManager* pManager, ElementID ID, std::size_t index, bool bInterior, bool bNative);
    ~CClientOcclusion();

    eClientEntityType GetType() const { return CCLIENTOCCLUSION; }
    void              Unlink() override {}

    std::size_t GetIndex() const noexcept { return m_index; }
    bool        IsInterior() const noexcept { return m_bInterior; }
    bool        IsNative() const noexcept { return m_bNative; }

    void GetPosition(CVector& vecPosition) const override;
    void SetPosition(const CVector& vecPosition) override;

    bool GetSize(CVector& vecSize) const;
    bool SetSize(const CVector& vecSize);

    bool GetRotation(CVector& vecRotation) const;
    bool SetRotation(const CVector& vecRotation);

    bool IsEnabled() const noexcept { return m_bEnabled; }
    bool SetEnabled(bool bEnabled);

    void DebugRender(const CVector& vecPosition, float fDrawRadius) override;

private:
    CClientOcclusionManager* m_pOcclusionManager;
    std::size_t              m_index;
    bool                     m_bInterior;
    bool                     m_bNative;
    bool                     m_bEnabled;
    CVector                  m_vecLastEnabledSize;  // remembered so SetEnabled(true) can restore it
};
