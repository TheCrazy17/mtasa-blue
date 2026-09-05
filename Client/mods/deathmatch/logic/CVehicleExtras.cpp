/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CVehicleExtras.cpp
 *  PURPOSE:     Vehicle extras (model-driven cosmetic add-ons, e.g. chains) class
 *
 *****************************************************************************/

#include <StdInc.h>
#include <algorithm>
#include <cmath>
#include <game/CClock.h>
#include "CVehicleExtras.h"
#include "CClientVehicle.h"
#include "lua/CLuaFunctionParseHelpers.h"

// A hanging chain is a hand-authored flipbook, not a physics sim: pick the next swing-position
// frame every so often, faster while the vehicle is moving faster. Ported from ModelExtras' chain
// feature, which calibrated these against the same native move-speed units CClientVehicle exposes.
namespace
{
    constexpr float kChainMinSpeed = 0.3f;
    constexpr float kChainMaxSpeed = 10.0f;
    constexpr float kChainMaxIntervalMs = 200.0f;
    constexpr float kChainMinIntervalMs = 20.0f;

    // A spoiler eases toward its target angle rather than snapping to it; it extends slower than it
    // retracts, matching ModelExtras' own tuning. speedMultiplier scales both rates, same meaning as
    // it has for chain: a bigger multiplier means the extra physically moves faster.
    constexpr float kSpoilerExtendRate = 10.0f;
    constexpr float kSpoilerRetractRate = 15.0f;

    // Gauge needles ease toward their target angle the same way spoiler does. ModelExtras itself applies
    // its coefficient as a direct per-frame increment (current += (target-current)*coefficient*delta)
    // rather than an exponential blend; the two are equivalent in the small-delta limit, and the blend
    // form is what this framework's own spoiler animation already established, so the same coefficients
    // are reused here as blend rates instead of duplicating a second smoothing style.
    constexpr float kRpmGaugeSmoothingRate = 0.25f;
    constexpr float kSpeedGaugeSmoothingRate = 0.5f;
    constexpr float kTurboGaugeSmoothingRate = 0.25f;
}  // namespace

std::unordered_map<CClientVehicle*, CVehicleExtras::VehicleExtraStates> CVehicleExtras::ms_VehicleStates;

bool CVehicleExtras::IsExtraSupported(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType)
{
    if (!pVehicle)
        return false;

    CModelInfo* pModelInfo = g_pGame->GetModelInfo(pVehicle->GetModel());
    return pModelInfo && pModelInfo->IsVehicleExtraSupported(eExtraType);
}

SVehicleExtraState& CVehicleExtras::GetState(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType)
{
    return ms_VehicleStates[pVehicle][eExtraType];
}

void CVehicleExtras::OnVehicleDestroy(CClientVehicle* pVehicle)
{
    ms_VehicleStates.erase(pVehicle);
}

bool CVehicleExtras::IsEnabled(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType)
{
    if (!IsExtraSupported(pVehicle, eExtraType))
        return false;

    return GetState(pVehicle, eExtraType).bEnabled;
}

bool CVehicleExtras::SetEnabled(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, bool bEnabled)
{
    if (!IsExtraSupported(pVehicle, eExtraType))
        return false;

    GetState(pVehicle, eExtraType).bEnabled = bEnabled;
    return true;
}

float CVehicleExtras::GetSpeedMultiplier(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType)
{
    if (!IsExtraSupported(pVehicle, eExtraType))
        return 1.0f;

    return GetState(pVehicle, eExtraType).fSpeedMultiplier;
}

bool CVehicleExtras::SetSpeedMultiplier(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, float fMultiplier)
{
    if (!IsExtraSupported(pVehicle, eExtraType) || fMultiplier <= 0.0f)
        return false;

    GetState(pVehicle, eExtraType).fSpeedMultiplier = fMultiplier;
    return true;
}

std::vector<SString> CVehicleExtras::GetAvailableExtras(CClientVehicle* pVehicle)
{
    std::vector<SString> results;
    if (!pVehicle)
        return results;

    CModelInfo* pModelInfo = g_pGame->GetModelInfo(pVehicle->GetModel());
    if (!pModelInfo)
        return results;

    for (int i = 0; i < VehicleExtraType::VEHICLE_EXTRA_TYPE_COUNT; i++)
    {
        auto eExtraType = static_cast<VehicleExtraType::Enum>(i);
        if (pModelInfo->IsVehicleExtraSupported(eExtraType))
            results.push_back(EnumToString(eExtraType));
    }

    return results;
}

void CVehicleExtras::Pulse(CClientVehicle* pVehicle)
{
    if (IsExtraSupported(pVehicle, VehicleExtraType::CHAIN))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::CHAIN);
        if (state.bEnabled)
            PulseChain(pVehicle, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::WHEEL_HUB))
    {
        if (GetState(pVehicle, VehicleExtraType::WHEEL_HUB).bEnabled)
            PulseWheelHub(pVehicle);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::SPOILER))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::SPOILER);
        if (state.bEnabled)
            PulseSpoiler(pVehicle, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::GEAR_INDICATOR))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::GEAR_INDICATOR);
        if (state.bEnabled)
            PulseGearIndicator(pVehicle, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::SPEED_GAUGE))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::SPEED_GAUGE);
        if (state.bEnabled)
            PulseGauge(pVehicle, VehicleExtraType::SPEED_GAUGE, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::RPM_GAUGE))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::RPM_GAUGE);
        if (state.bEnabled)
            PulseGauge(pVehicle, VehicleExtraType::RPM_GAUGE, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::TURBO_GAUGE))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::TURBO_GAUGE);
        if (state.bEnabled)
            PulseGauge(pVehicle, VehicleExtraType::TURBO_GAUGE, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::FIXED_GAUGE))
    {
        if (GetState(pVehicle, VehicleExtraType::FIXED_GAUGE).bEnabled)
            PulseFixedGauge(pVehicle);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::ODOMETER))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::ODOMETER);
        if (state.bEnabled)
            PulseOdometer(pVehicle, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::CLOCK))
    {
        if (GetState(pVehicle, VehicleExtraType::CLOCK).bEnabled)
            PulseClock(pVehicle);
    }
}

void CVehicleExtras::PulseChain(CClientVehicle* pVehicle, SVehicleExtraState& state)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    std::size_t frameCount = pGameVehicle->GetVehicleExtraFrameCount(VehicleExtraType::CHAIN);
    if (frameCount == 0)
        return;

    CVector vecMoveSpeed;
    pVehicle->GetMoveSpeed(vecMoveSpeed);

    CMatrix matVehicle;
    pVehicle->GetMatrix(matVehicle);

    // Signed, direction-aware speed: positive is travelling forward, negative is reversing
    float speed = matVehicle.vFront.DotProduct(&vecMoveSpeed);
    float absSpeed = std::fabs(speed);

    float multiplier = state.fSpeedMultiplier;
    float interval = kChainMaxIntervalMs;
    if (absSpeed > kChainMinSpeed)
    {
        float t = std::clamp((absSpeed - kChainMinSpeed) / (kChainMaxSpeed - kChainMinSpeed), 0.0f, 1.0f);
        interval = std::lerp(kChainMaxIntervalMs, kChainMinIntervalMs, t);
    }
    interval /= multiplier;

    if ((CTickCount::Now() - state.lastUpdateTime).ToInt() < interval)
        return;

    std::int16_t maxIndex = static_cast<std::int16_t>(frameCount - 1);

    if (pVehicle->GetVehicleType() == CLIENTVEHICLE_BMX)
    {
        // BMX chains only swing while actively pedaled forward; coasting or braking leaves them still
        if (pVehicle->GetGasPedal() > 0.0f && speed > 0.0f)
            state.sCurrentFrame = (state.sCurrentFrame == 0) ? maxIndex : state.sCurrentFrame - 1;
    }
    else
    {
        if (speed > kChainMinSpeed)
            state.sCurrentFrame = (state.sCurrentFrame == 0) ? maxIndex : state.sCurrentFrame - 1;
        else if (speed < -kChainMinSpeed)
            state.sCurrentFrame = (state.sCurrentFrame == maxIndex) ? 0 : state.sCurrentFrame + 1;
    }

    pGameVehicle->SetVehicleExtraFrame(VehicleExtraType::CHAIN, static_cast<std::size_t>(state.sCurrentFrame));
    state.lastUpdateTime = CTickCount::Now();
}

void CVehicleExtras::PulseWheelHub(CClientVehicle* pVehicle)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    // The rotation copy is pure overhead if nobody can see it
    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleExtraWheelHubs();
}

void CVehicleExtras::PulseSpoiler(CClientVehicle* pVehicle, SVehicleExtraState& state)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    std::size_t spoilerCount = pGameVehicle->GetVehicleSpoilerCount();
    if (spoilerCount == 0)
        return;

    CVector vecMoveSpeed;
    pVehicle->GetMoveSpeed(vecMoveSpeed);

    // ModelExtras compares against CarUtil::GetVehicleSpeed, the 2D move-speed magnitude scaled by
    // 50; match that scale so a dummy-name-encoded trigger speed keeps its original real-world meaning
    float speed = std::sqrt(vecMoveSpeed.fX * vecMoveSpeed.fX + vecMoveSpeed.fY * vecMoveSpeed.fY) * 50.0f;
    float timeStep = g_pGame->GetTimeStep();

    for (std::size_t i = 0; i < spoilerCount; i++)
    {
        float fRotationDegrees, fTransitionTime, fTriggerSpeed;
        if (!pGameVehicle->GetVehicleSpoilerConfig(i, fRotationDegrees, fTransitionTime, fTriggerSpeed))
            continue;

        bool  bIsTriggered = speed > fTriggerSpeed;
        float fTargetAngle = bIsTriggered ? -fRotationDegrees : 0.0f;
        float fTotalTime = std::max(1.0f, fTransitionTime);

        // speedMultiplier scales how fast the spoiler physically moves, same meaning it has for chain
        float fTransitionSpeed = (bIsTriggered ? kSpoilerExtendRate : kSpoilerRetractRate) / fTotalTime * state.fSpeedMultiplier;

        // Framerate-independent exponential smoothing toward the target angle
        float fBlend = 1.0f - std::exp(-fTransitionSpeed * timeStep);

        float fCurrentAngle = pGameVehicle->GetVehicleSpoilerAngle(i);
        fCurrentAngle = fCurrentAngle * (1.0f - fBlend) + fTargetAngle * fBlend;

        pGameVehicle->SetVehicleSpoilerAngle(i, fCurrentAngle);
    }
}

void CVehicleExtras::PulseGearIndicator(CClientVehicle* pVehicle, SVehicleExtraState& state)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    std::size_t frameCount = pGameVehicle->GetVehicleExtraFrameCount(VehicleExtraType::GEAR_INDICATOR);
    if (frameCount == 0)
        return;

    auto sCurrentGear = static_cast<std::int16_t>(pGameVehicle->GetCurrentGear());
    if (sCurrentGear == state.sCurrentFrame || static_cast<std::size_t>(sCurrentGear) >= frameCount)
        return;

    pGameVehicle->SetVehicleExtraFrame(VehicleExtraType::GEAR_INDICATOR, static_cast<std::size_t>(sCurrentGear));
    state.sCurrentFrame = sCurrentGear;
}

void CVehicleExtras::PulseGauge(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, SVehicleExtraState& state)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    std::size_t gaugeCount = pGameVehicle->GetVehicleGaugeCount(eExtraType);
    if (gaugeCount == 0)
        return;

    float fSmoothingRate = kRpmGaugeSmoothingRate;
    if (eExtraType == VehicleExtraType::SPEED_GAUGE)
        fSmoothingRate = kSpeedGaugeSmoothingRate;
    else if (eExtraType == VehicleExtraType::TURBO_GAUGE)
        fSmoothingRate = kTurboGaugeSmoothingRate;

    float fTimeStep = g_pGame->GetTimeStep();

    for (std::size_t i = 0; i < gaugeCount; i++)
    {
        float fTargetAngle;
        if (!pGameVehicle->GetVehicleGaugeTargetAngle(eExtraType, i, fTargetAngle))
            continue;

        // speedMultiplier scales how fast the needle physically moves, same meaning it has for chain/spoiler
        float fBlend = std::clamp(fSmoothingRate * fTimeStep * state.fSpeedMultiplier, 0.0f, 1.0f);

        float fCurrentAngle = pGameVehicle->GetVehicleGaugeAngle(eExtraType, i);
        fCurrentAngle = fCurrentAngle * (1.0f - fBlend) + fTargetAngle * fBlend;

        pGameVehicle->SetVehicleGaugeAngle(eExtraType, i, fCurrentAngle);
    }
}

void CVehicleExtras::PulseFixedGauge(CClientVehicle* pVehicle)
{
    // FixedGauge's whole behaviour (a one-time randomised resting angle) happens inside
    // CVehicleSA::GetVehicleGaugeCount's own lazy-resolve step; this just needs to trigger that once
    pVehicle->GetGameVehicle()->GetVehicleGaugeCount(VehicleExtraType::FIXED_GAUGE);
}

void CVehicleExtras::PulseOdometer(CClientVehicle* pVehicle, SVehicleExtraState& state)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleOdometer(state.fSpeedMultiplier);
}

void CVehicleExtras::PulseClock(CClientVehicle* pVehicle)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    unsigned char ucHour = 0;
    unsigned char ucMinute = 0;
    g_pGame->GetClock()->Get(&ucHour, &ucMinute);

    // ModelExtras supports a per-dummy 12-hour toggle via its own external per-model JSON config; this
    // framework has no equivalent config layer, so the clock always shows 24-hour time
    ucHour %= 24;
    ucMinute %= 60;

    pGameVehicle->SetClockDigits(static_cast<std::uint8_t>(ucHour / 10), static_cast<std::uint8_t>(ucHour % 10), static_cast<std::uint8_t>(ucMinute / 10),
                                 static_cast<std::uint8_t>(ucMinute % 10));
}
