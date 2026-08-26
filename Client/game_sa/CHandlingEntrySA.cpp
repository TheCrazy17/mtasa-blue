/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        Client/game_sa/CHandlingEntrySA.cpp
 *  PURPOSE:     Vehicle handling data entry
 *
 *  Multi Theft Auto is available from https://multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGameSA.h"
#include "CHandlingEntrySA.h"
#include "CHandlingManagerSA.h"

extern CGameSA* pGame;

CHandlingEntrySA::CHandlingEntrySA()
{
    // Create a new interface and zero it
    if (m_HandlingSA = std::make_unique<tHandlingDataSA>())
    {
        MemSetFast(m_HandlingSA.get(), 0, sizeof(tHandlingDataSA));
    }
}

CHandlingEntrySA::CHandlingEntrySA(const tHandlingDataSA* const pOriginal)
{
    // Store gta's pointer
    m_HandlingSA = nullptr;
    if (pOriginal)
    {
        MemCpyFast(&m_Handling, pOriginal, sizeof(tHandlingDataSA));
    }
}

// Apply the handlingdata from another data
void CHandlingEntrySA::Assign(const CHandlingEntry* const pEntry) noexcept
{
    if (!pEntry)
        return;

    // Copy the data
    const CHandlingEntrySA* const pEntrySA = static_cast<const CHandlingEntrySA*>(pEntry);
    m_Handling = pEntrySA->m_Handling;
}

void CHandlingEntrySA::Recalculate() noexcept
{
    // Real GTA class?
    if (!m_HandlingSA)
        return;

    // Copy our stored field to GTA's
    MemCpyFast(m_HandlingSA.get(), &m_Handling, sizeof(m_Handling));
    ((void(_stdcall*)(tHandlingDataSA*))FUNC_HandlingDataMgr_ConvertDataToGameUnits)(m_HandlingSA.get());

    // The call above just worked out fMaxReverseVelocity as whichever is smaller, i.e. more
    // negative, of two things: 30% of the forward top speed it calculated right before that, or a
    // flat -0.2. That flat fallback only exists so a vehicle can still back up at all in case the
    // proportional figure would round down to almost nothing; but plenty of stock vehicles already
    // have a forward top speed under a third of that (tractors, forklifts, the RC Tiger, ...), and
    // setVehicleHandling makes it trivial to push any vehicle below that point on purpose. Once
    // that happens, the flat fallback wins every time, so reverse stops scaling with maxVelocity at
    // all: the vehicle keeps reversing at the same speed no matter how low maxVelocity is set,
    // while its forward speed keeps dropping. Recompute it here without that floor, so reverse
    // tracks the forward cap the same way for every regular vehicle, the same way the game already
    // does for fast ones. The RC Bandit (reverses exactly as fast as it drives forward) and
    // two-wheelers (a fixed, tiny reverse creep instead of a proportional one) are already special
    // cased by GTA itself; leave both of those exactly as the game set them up.
    CTransmissionSAInterface& transmission = m_HandlingSA->Transmission;
    const auto                eVehicleID = static_cast<HandlingType>(m_HandlingSA->iVehicleID);
    const bool                bIsTwoWheeler = eVehicleID >= HandlingType::HT_BIKE && eVehicleID <= HandlingType::HT_FREEWAY;
    if (eVehicleID != HandlingType::HT_RCBANDIT && !bIsTwoWheeler)
    {
        transmission.fMaxReverseVelocity = transmission.fMaxFlatVelocity * -0.3f;

        // The reverse gear's entry in the gear ratio table is also derived from
        // fMaxReverseVelocity, but only gets filled in when InitGearRatios runs; it already ran
        // once inside ConvertDataToGameUnits with the old value, so run it again now that
        // fMaxReverseVelocity has been corrected.
        ((void(__thiscall*)(CTransmissionSAInterface*))FUNC_Transmission_InitGearRatios)(&transmission);
    }
}

void CHandlingEntrySA::SetSuspensionForceLevel(float fForce) noexcept
{
    if (!std::isfinite(fForce)) [[unlikely]]
        return;
    CheckSuspensionChanges();
    m_Handling.fSuspensionForceLevel = fForce;
}

void CHandlingEntrySA::SetSuspensionDamping(float fDamping) noexcept
{
    if (!std::isfinite(fDamping)) [[unlikely]]
        return;
    CheckSuspensionChanges();
    m_Handling.fSuspensionDamping = fDamping;
}

void CHandlingEntrySA::SetSuspensionHighSpeedDamping(float fDamping) noexcept
{
    if (!std::isfinite(fDamping)) [[unlikely]]
        return;
    CheckSuspensionChanges();
    m_Handling.fSuspensionHighSpdDamping = fDamping;
}

void CHandlingEntrySA::SetSuspensionUpperLimit(float fUpperLimit) noexcept
{
    if (!std::isfinite(fUpperLimit)) [[unlikely]]
        return;
    CheckSuspensionChanges();
    m_Handling.fSuspensionUpperLimit = fUpperLimit;
}

void CHandlingEntrySA::SetSuspensionLowerLimit(float fLowerLimit) noexcept
{
    if (!std::isfinite(fLowerLimit)) [[unlikely]]
        return;
    CheckSuspensionChanges();
    m_Handling.fSuspensionLowerLimit = fLowerLimit;
}

void CHandlingEntrySA::SetSuspensionFrontRearBias(float fBias) noexcept
{
    if (!std::isfinite(fBias)) [[unlikely]]
        return;
    CheckSuspensionChanges();
    m_Handling.fSuspensionFrontRearBias = fBias;
}

void CHandlingEntrySA::SetSuspensionAntiDiveMultiplier(float fAntidive) noexcept
{
    if (!std::isfinite(fAntidive)) [[unlikely]]
        return;
    CheckSuspensionChanges();
    m_Handling.fSuspensionAntiDiveMultiplier = fAntidive;
}

void CHandlingEntrySA::CheckSuspensionChanges() const noexcept
{
    pGame->GetHandlingManager()->CheckSuspensionChanges(this);
}
