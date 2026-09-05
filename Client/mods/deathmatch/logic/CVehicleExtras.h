/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CVehicleExtras.h
 *  PURPOSE:     Vehicle extras (model-driven cosmetic add-ons, e.g. chains) class header
 *
 *****************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "enums/VehicleExtraType.h"

class CClientVehicle;
class SString;

// Per-vehicle, per-extra state a script can see or tune. Kept out of CClientVehicle itself since
// most vehicles never use any extra; only vehicles actually touched get an entry.
struct SVehicleExtraState
{
    bool         bEnabled = true;
    float        fSpeedMultiplier = 1.0f;
    std::int16_t sCurrentFrame = 0;
    CTickCount   lastUpdateTime;
};

// Drives the model-driven vehicle extras framework (chain, rotating wheel hubs, animated spoiler so
// far; doors/roof can follow the same pattern: detect the dummy in CModelInfoSA, resolve frames in
// CVehicleSA, add a case here).
class CVehicleExtras
{
public:
    using VehicleExtraStates = std::array<SVehicleExtraState, VehicleExtraType::VEHICLE_EXTRA_TYPE_COUNT>;

    static void Pulse(CClientVehicle* pVehicle);
    static void OnVehicleDestroy(CClientVehicle* pVehicle);

    static bool IsEnabled(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType);
    static bool SetEnabled(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, bool bEnabled);

    static float GetSpeedMultiplier(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType);
    static bool  SetSpeedMultiplier(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, float fMultiplier);

    static std::vector<SString> GetAvailableExtras(CClientVehicle* pVehicle);

private:
    static bool                IsExtraSupported(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType);
    static SVehicleExtraState& GetState(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType);

    static void PulseChain(CClientVehicle* pVehicle, SVehicleExtraState& state);
    static void PulseWheelHub(CClientVehicle* pVehicle);
    static void PulseSpoiler(CClientVehicle* pVehicle, SVehicleExtraState& state);
    static void PulseGearIndicator(CClientVehicle* pVehicle, SVehicleExtraState& state);
    static void PulseGauge(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, SVehicleExtraState& state);
    static void PulseFixedGauge(CClientVehicle* pVehicle);
    static void PulseOdometer(CClientVehicle* pVehicle, SVehicleExtraState& state);
    static void PulseClock(CClientVehicle* pVehicle);

    static std::unordered_map<CClientVehicle*, VehicleExtraStates> ms_VehicleStates;
};
