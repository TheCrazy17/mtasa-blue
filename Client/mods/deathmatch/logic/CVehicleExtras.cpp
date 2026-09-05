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
