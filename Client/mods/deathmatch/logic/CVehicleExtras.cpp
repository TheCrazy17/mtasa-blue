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
#include <game/CDamageManager.h>
#include <game/CDoor.h>
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

    // A light on/off threshold for pedal-driven extras (brake, a door counting as "open"); small enough
    // that a barely-touched pedal or a just-cracked door still counts, matching how a real one behaves.
    constexpr float kLightOnThreshold = 0.05f;

    // FOG_LIGHT, SPOTLIGHT, INDICATOR_LEFT and INDICATOR_RIGHT have no automatic native trigger (see
    // CVehicleExtras::Pulse) and ARE the on/off switch themselves rather than a gate on automatic
    // behaviour, so - unlike every other extra's bEnabled, which defaults true because its automatic
    // behaviour should just run out of the box - they need to start off (no vehicle should spawn with
    // its indicators already blinking). SVehicleExtraState::bTargetOpen already defaults false and is
    // otherwise unused outside convertible roof/rollback bed, so it is reused here as this group's
    // storage instead of adding a new field just to get a different default.
    bool UsesTargetOpenAsSwitch(VehicleExtraType::Enum eExtraType)
    {
        return eExtraType == VehicleExtraType::FOG_LIGHT || eExtraType == VehicleExtraType::SPOTLIGHT ||
               eExtraType == VehicleExtraType::INDICATOR_LEFT || eExtraType == VehicleExtraType::INDICATOR_RIGHT;
    }

    // ModelExtras' own lights/damage.h: a damaged light or its surrounding body panel should stop that
    // light from lighting up. CVehicle::GetDamageManager() already wraps GTA's own native per-light/
    // per-panel damage tracking - sdk/game/CDamageManager.h's eLights/ePanels are the same four real
    // lights (GTA never tracked brake or indicator bulbs separately either) and seven real panels
    // ModelExtras' own eLights/ePanels enumerate, just spelled differently - so this is a straight
    // translation, not new infrastructure. ModelExtras itself also only trusts this state for
    // CAutomobile: CarUtil::IsLightDamaged/IsPanelDamaged both report "not damaged" for every other
    // subclass, since only a car's damage manager bytes are meaningfully panel/light state there;
    // mirrored here as CLIENTVEHICLE_CAR.
    bool IsHeadlightOk(CClientVehicle* pVehicle, CVehicle* pGameVehicle, bool bLeft)
    {
        if (pVehicle->GetVehicleType() != CLIENTVEHICLE_CAR)
            return true;

        CDamageManager* pDamage = pGameVehicle->GetDamageManager();
        return !pDamage->GetLightStatus(bLeft ? LEFT_HEADLIGHT : RIGHT_HEADLIGHT) &&
               !pDamage->GetPanelStatus(bLeft ? FRONT_LEFT_PANEL : FRONT_RIGHT_PANEL);
    }

    bool IsTaillightOk(CClientVehicle* pVehicle, CVehicle* pGameVehicle, bool bLeft)
    {
        if (pVehicle->GetVehicleType() != CLIENTVEHICLE_CAR)
            return true;

        CDamageManager* pDamage = pGameVehicle->GetDamageManager();
        return !pDamage->GetLightStatus(bLeft ? LEFT_TAIL_LIGHT : RIGHT_TAIL_LIGHT) &&
               !pDamage->GetPanelStatus(bLeft ? REAR_LEFT_PANEL : REAR_RIGHT_PANEL);
    }

    // SIDE_LIGHT keys off the front wing panel only (matching ModelExtras' own isMiddleLeftOk/
    // isMiddleRightOk), not the rear one, despite upstream's own "middle" naming for this check
    bool IsFrontWingOk(CClientVehicle* pVehicle, CVehicle* pGameVehicle, bool bLeft)
    {
        if (pVehicle->GetVehicleType() != CLIENTVEHICLE_CAR)
            return true;

        return !pGameVehicle->GetDamageManager()->GetPanelStatus(bLeft ? FRONT_LEFT_PANEL : FRONT_RIGHT_PANEL);
    }

    bool IsFrontBumperOk(CClientVehicle* pVehicle, CVehicle* pGameVehicle)
    {
        if (pVehicle->GetVehicleType() != CLIENTVEHICLE_CAR)
            return true;

        return !pGameVehicle->GetDamageManager()->GetPanelStatus(FRONT_BUMPER);
    }

    // isFrontLeftOk/isFrontRightOk in ModelExtras' own damage.h: a siren/alarm-equipped vehicle's front
    // indicator doesn't share the headlight's damage state. Used here as this branch's own single
    // representative check for INDICATOR_LEFT/RIGHT, which - unlike upstream's own front/middle/rear
    // split - aren't broken out by position, so they can't carry three different per-position checks
    bool IsFrontIndicatorOk(CClientVehicle* pVehicle, CVehicle* pGameVehicle, bool bLeft)
    {
        return IsHeadlightOk(pVehicle, pGameVehicle, bLeft) || pVehicle->IsSirenOrAlarmActive();
    }
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

    if (UsesTargetOpenAsSwitch(eExtraType))
        return GetState(pVehicle, eExtraType).bTargetOpen;

    return GetState(pVehicle, eExtraType).bEnabled;
}

bool CVehicleExtras::SetEnabled(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, bool bEnabled)
{
    if (!IsExtraSupported(pVehicle, eExtraType))
        return false;

    if (UsesTargetOpenAsSwitch(eExtraType))
        GetState(pVehicle, eExtraType).bTargetOpen = bEnabled;
    else
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

    if (IsExtraSupported(pVehicle, VehicleExtraType::EXTRA_WHEEL))
    {
        if (GetState(pVehicle, VehicleExtraType::EXTRA_WHEEL).bEnabled)
            PulseExtraWheel(pVehicle);
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

    if (IsExtraSupported(pVehicle, VehicleExtraType::ROTATE_DOOR))
    {
        if (GetState(pVehicle, VehicleExtraType::ROTATE_DOOR).bEnabled)
            PulseRotateDoor(pVehicle);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::SLIDE_DOOR))
    {
        if (GetState(pVehicle, VehicleExtraType::SLIDE_DOOR).bEnabled)
            PulseSlideDoor(pVehicle);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::CONVERTIBLE_ROOF))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::CONVERTIBLE_ROOF);
        if (state.bEnabled)
            PulseConvertibleRoof(pVehicle, state);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::ROLLBACK_BED))
    {
        SVehicleExtraState& state = GetState(pVehicle, VehicleExtraType::ROLLBACK_BED);
        if (state.bEnabled)
            PulseRollbackBed(pVehicle, state);
    }

    // Lights: each reacts to the same real vehicle state a real one would, the same automatic-by-default
    // philosophy as every extra above - see CVehicleSA::GetVehicleLightFrameCount for the dummy names.
    // Only fog light, spotlight and the two indicators have no real state to read (GTA tracks neither a
    // fog light nor a turn signal at all - confirmed against gta-reversed, not assumed), so those four
    // are manual, driven by setVehicleExtraEnabled/isVehicleExtraEnabled same as every other extra's
    // generic API; nothing new was added to it.
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (IsExtraSupported(pVehicle, VehicleExtraType::HEADLIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::HEADLIGHT).bEnabled)
        {
            // The dummy shows/hides as one combined group with no left/right split (see
            // g_LightDummyPrefixes), so a single side being damaged can't hide just that side - showing
            // as long as either headlight is intact avoids hiding a working lamp over its damaged partner
            bool bOk = IsHeadlightOk(pVehicle, pGameVehicle, true) || IsHeadlightOk(pVehicle, pGameVehicle, false);
            PulseSimpleLight(pVehicle, VehicleExtraType::HEADLIGHT, pGameVehicle->GetLightsOn() && bOk);
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::TAIL_LIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::TAIL_LIGHT).bEnabled)
        {
            bool bOk = IsTaillightOk(pVehicle, pGameVehicle, true) || IsTaillightOk(pVehicle, pGameVehicle, false);
            PulseSimpleLight(pVehicle, VehicleExtraType::TAIL_LIGHT, pGameVehicle->GetLightsOn() && bOk);
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::BRAKE_LIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::BRAKE_LIGHT).bEnabled)
        {
            bool bOk = IsTaillightOk(pVehicle, pGameVehicle, true) || IsTaillightOk(pVehicle, pGameVehicle, false);
            PulseSimpleLight(pVehicle, VehicleExtraType::BRAKE_LIGHT, pGameVehicle->GetBrakePedal() > kLightOnThreshold && bOk);
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::REVERSE_LIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::REVERSE_LIGHT).bEnabled)
        {
            // Confirmed against gta-reversed (AEVehicleAudioEntity.cpp's own "are we reversing?" check):
            // GTA:SA uses gear 0 for reverse, and treats a negative gas pedal the same way
            bool bReversing = pGameVehicle->IsEngineOn() && (pGameVehicle->GetCurrentGear() == 0 || pGameVehicle->GetGasPedal() < 0.0f);
            bool bOk = IsTaillightOk(pVehicle, pGameVehicle, true) || IsTaillightOk(pVehicle, pGameVehicle, false);
            PulseSimpleLight(pVehicle, VehicleExtraType::REVERSE_LIGHT, bReversing && bOk);
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::SIDE_LIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::SIDE_LIGHT).bEnabled)
        {
            bool bOk = IsFrontWingOk(pVehicle, pGameVehicle, true) || IsFrontWingOk(pVehicle, pGameVehicle, false);
            PulseSimpleLight(pVehicle, VehicleExtraType::SIDE_LIGHT, pGameVehicle->GetLightsOn() && bOk);
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::FOG_LIGHT))
        PulseSimpleLight(pVehicle, VehicleExtraType::FOG_LIGHT, IsEnabled(pVehicle, VehicleExtraType::FOG_LIGHT) && IsFrontBumperOk(pVehicle, pGameVehicle));

    if (IsExtraSupported(pVehicle, VehicleExtraType::DRL))
    {
        if (GetState(pVehicle, VehicleExtraType::DRL).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::DRL, pGameVehicle->IsEngineOn());
    }

    // Strobe blinks unconditionally whenever the model has the dummy - ModelExtras' own StrobeLightComponent
    // has no on/off state or vehicle-state gate at all (unlike every light above), just a per-dummy timer
    // that flips every 1000ms (strobe.delay's own default; this framework has no equivalent of ModelExtras'
    // external per-model JSON override for it, same as every other JSON-only tunable already dropped
    // elsewhere on this branch). Every strobe dummy on a vehicle starts that timer at the same value (0),
    // so upstream's own dummies already flip in lockstep - the same wall-clock-phase shape indicators
    // already use here reproduces that synchronised blink without needing per-instance timer state
    if (IsExtraSupported(pVehicle, VehicleExtraType::STROBE_LIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::STROBE_LIGHT).bEnabled)
        {
            bool bStrobePhase = (CTickCount::Now().ToLongLong() / 1000) % 2 == 0;
            PulseSimpleLight(pVehicle, VehicleExtraType::STROBE_LIGHT, bStrobePhase);
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::SPOTLIGHT))
    {
        PulseSimpleLight(pVehicle, VehicleExtraType::SPOTLIGHT, IsEnabled(pVehicle, VehicleExtraType::SPOTLIGHT));

        // Aiming runs independently of the on/off switch above, matching ModelExtras' own
        // OnHudRender: the aim dummy tracks the camera whenever the control is held, whether or not
        // the light is currently switched on
        PulseSpotlightAim(pVehicle);
    }

    // Indicators blink in step across every vehicle, the same wall-clock 500ms flip ModelExtras' own
    // global BlinkerState uses; computed from the clock itself instead of stored per-vehicle state, so
    // there is nothing to keep in sync or reset
    bool bIndicatorBlinkPhase = (CTickCount::Now().ToLongLong() / 500) % 2 == 0;
    bool bIndicatorLeftOn = false;
    bool bIndicatorRightOn = false;

    if (IsExtraSupported(pVehicle, VehicleExtraType::INDICATOR_LEFT))
    {
        bIndicatorLeftOn = IsEnabled(pVehicle, VehicleExtraType::INDICATOR_LEFT);
        PulseSimpleLight(pVehicle, VehicleExtraType::INDICATOR_LEFT,
                          bIndicatorLeftOn && bIndicatorBlinkPhase && IsFrontIndicatorOk(pVehicle, pGameVehicle, true));
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::INDICATOR_RIGHT))
    {
        bIndicatorRightOn = IsEnabled(pVehicle, VehicleExtraType::INDICATOR_RIGHT);
        PulseSimpleLight(pVehicle, VehicleExtraType::INDICATOR_RIGHT,
                          bIndicatorRightOn && bIndicatorBlinkPhase && IsFrontIndicatorOk(pVehicle, pGameVehicle, false));
    }

    // STT (stop/tail/turn, ModelExtras' STTLightComponent) is a combined lamp: lit for brake or tail duty
    // the same way BRAKE_LIGHT/TAIL_LIGHT above already are, plus - ModelExtras' own IndicatorComponent
    // additionally renders the very same STTLightLeft/Right material while that side's indicator is
    // blinking - the indicator's own blink phase on top. Left/right need independent state here (unlike
    // every combined light above) because both the indicator interaction and the damage gating genuinely
    // differ per side.
    bool bBrakeOn = pGameVehicle->GetBrakePedal() > kLightOnThreshold;
    bool bTailOn = pGameVehicle->GetLightsOn();

    if (IsExtraSupported(pVehicle, VehicleExtraType::STT_LIGHT_LEFT))
    {
        if (GetState(pVehicle, VehicleExtraType::STT_LIGHT_LEFT).bEnabled)
        {
            bool bOn = bBrakeOn || bTailOn || (bIndicatorLeftOn && bIndicatorBlinkPhase);
            PulseSimpleLight(pVehicle, VehicleExtraType::STT_LIGHT_LEFT, bOn && IsTaillightOk(pVehicle, pGameVehicle, true));
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::STT_LIGHT_RIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::STT_LIGHT_RIGHT).bEnabled)
        {
            bool bOn = bBrakeOn || bTailOn || (bIndicatorRightOn && bIndicatorBlinkPhase);
            PulseSimpleLight(pVehicle, VehicleExtraType::STT_LIGHT_RIGHT, bOn && IsTaillightOk(pVehicle, pGameVehicle, false));
        }
    }

    // NABRAKE (non-actuated brake accent, ModelExtras' NABrakeLightComponent): a brake-only accent lamp
    // next to a dedicated indicator, so it has to go dark - not blink - for as long as this side's
    // indicator is switched on, leaving the indicator itself as the only thing lit there; otherwise the
    // two would show together and muddy the signal. ModelExtras' own gate compares its single combined
    // indicator-state enum against BothOn/LeftOn/RightOn; with this branch's own independent per-side
    // indicator switches that reduces to "this side's own indicator switch is off", checked directly
    // rather than reintroducing a combined state just to compare against.
    if (IsExtraSupported(pVehicle, VehicleExtraType::NABRAKE_LIGHT_LEFT))
    {
        if (GetState(pVehicle, VehicleExtraType::NABRAKE_LIGHT_LEFT).bEnabled)
        {
            bool bOn = bBrakeOn && !bIndicatorLeftOn;
            PulseSimpleLight(pVehicle, VehicleExtraType::NABRAKE_LIGHT_LEFT, bOn && IsTaillightOk(pVehicle, pGameVehicle, true));
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::NABRAKE_LIGHT_RIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::NABRAKE_LIGHT_RIGHT).bEnabled)
        {
            bool bOn = bBrakeOn && !bIndicatorRightOn;
            PulseSimpleLight(pVehicle, VehicleExtraType::NABRAKE_LIGHT_RIGHT, bOn && IsTaillightOk(pVehicle, pGameVehicle, false));
        }
    }

    // Dashboard LEDs: same shape as the lights above (a dummy mesh shown or hidden as one group), but
    // for an interior indicator bulb reacting to the same state rather than the light itself. This is a
    // new x_led_ dummy convention this port introduces, not ModelExtras' own LEDs feature, which detects
    // these purely by material colour instead - see this feature's own report for why.
    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_ENGINE_ON))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_ENGINE_ON).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_ENGINE_ON, pGameVehicle->IsEngineOn());
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_ENGINE_BROKEN))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_ENGINE_BROKEN).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_ENGINE_BROKEN, pGameVehicle->IsEngineBroken());
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_FOG_LIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_FOG_LIGHT).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_FOG_LIGHT, IsEnabled(pVehicle, VehicleExtraType::FOG_LIGHT));
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_HEADLIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_HEADLIGHT).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_HEADLIGHT, pGameVehicle->GetLightsOn());
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_INDICATOR_LEFT))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_INDICATOR_LEFT).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_INDICATOR_LEFT, bIndicatorLeftOn && bIndicatorBlinkPhase);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_INDICATOR_RIGHT))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_INDICATOR_RIGHT).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_INDICATOR_RIGHT, bIndicatorRightOn && bIndicatorBlinkPhase);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_SIREN))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_SIREN).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_SIREN, pVehicle->IsSirenOrAlarmActive());
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_DOOR_OPEN))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_DOOR_OPEN).bEnabled)
        {
            bool bAnyDoorOpen = pGameVehicle->GetDoor(FRONT_LEFT_DOOR)->GetAngleOpenRatio() > kLightOnThreshold ||
                                pGameVehicle->GetDoor(FRONT_RIGHT_DOOR)->GetAngleOpenRatio() > kLightOnThreshold ||
                                pGameVehicle->GetDoor(REAR_LEFT_DOOR)->GetAngleOpenRatio() > kLightOnThreshold ||
                                pGameVehicle->GetDoor(REAR_RIGHT_DOOR)->GetAngleOpenRatio() > kLightOnThreshold;
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_DOOR_OPEN, bAnyDoorOpen);
        }
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_BONNET_OPEN))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_BONNET_OPEN).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_BONNET_OPEN, pGameVehicle->GetDoor(BONNET)->GetAngleOpenRatio() > kLightOnThreshold);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_BOOT_OPEN))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_BOOT_OPEN).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_BOOT_OPEN, pGameVehicle->GetDoor(BOOT)->GetAngleOpenRatio() > kLightOnThreshold);
    }

    if (IsExtraSupported(pVehicle, VehicleExtraType::LED_ROOF_OPEN))
    {
        if (GetState(pVehicle, VehicleExtraType::LED_ROOF_OPEN).bEnabled)
            PulseSimpleLight(pVehicle, VehicleExtraType::LED_ROOF_OPEN, IsOpen(pVehicle, VehicleExtraType::CONVERTIBLE_ROOF));
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

void CVehicleExtras::PulseExtraWheel(CClientVehicle* pVehicle)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    // The rotation copy is pure overhead if nobody can see it
    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleExtraWheels();
}

void CVehicleExtras::PulseSimpleLight(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, bool bVisible)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    if (pGameVehicle->GetVehicleLightFrameCount(eExtraType) == 0)
        return;

    pGameVehicle->SetVehicleLightVisible(eExtraType, bVisible);
}

void CVehicleExtras::PulseSpotlightAim(CClientVehicle* pVehicle)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleSpotlightAim();
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

void CVehicleExtras::PulseRotateDoor(CClientVehicle* pVehicle)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleExtraRotateDoors();
}

void CVehicleExtras::PulseSlideDoor(CClientVehicle* pVehicle)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleExtraSlideDoors();
}

void CVehicleExtras::PulseConvertibleRoof(CClientVehicle* pVehicle, SVehicleExtraState& state)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleExtraConvertibleRoof(state.bTargetOpen, state.fSpeedMultiplier);
}

void CVehicleExtras::PulseRollbackBed(CClientVehicle* pVehicle, SVehicleExtraState& state)
{
    CVehicle* pGameVehicle = pVehicle->GetGameVehicle();

    if (!pGameVehicle->IsOnScreen())
        return;

    pGameVehicle->UpdateVehicleExtraRollbackBed(state.bTargetOpen, state.fSpeedMultiplier);
}

bool CVehicleExtras::IsOpen(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType)
{
    if (!IsExtraSupported(pVehicle, eExtraType))
        return false;

    return GetState(pVehicle, eExtraType).bTargetOpen;
}

bool CVehicleExtras::SetOpen(CClientVehicle* pVehicle, VehicleExtraType::Enum eExtraType, bool bOpen)
{
    if (!IsExtraSupported(pVehicle, eExtraType))
        return false;

    // Only these two extras are open/close state machines; every other type has no such concept
    if (eExtraType != VehicleExtraType::CONVERTIBLE_ROOF && eExtraType != VehicleExtraType::ROLLBACK_BED)
        return false;

    // A rollback bed's hydraulics need the engine running to move in either direction, matching
    // ModelExtras' own toggle handler
    if (eExtraType == VehicleExtraType::ROLLBACK_BED && !pVehicle->IsEngineOn())
        return false;

    GetState(pVehicle, eExtraType).bTargetOpen = bOpen;
    return true;
}
