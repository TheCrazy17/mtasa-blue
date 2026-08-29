/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CAutomobileSA.cpp
 *  PURPOSE:     Automobile vehicle entity
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CAutomobileSA.h"
#include "CGameSA.h"

extern CGameSA* pGame;

CAutomobileSA::CAutomobileSA(CAutomobileSAInterface* pInterface)
{
    SetInterface(pInterface);
    Init();
}

// Returns true when any wheel is partially compressed (not fully relaxed)
bool CAutomobileSA::IsAnyWheelTouchingGround() const
{
    CAutomobileSAInterface* autoInterface = GetAutomobileInterface();
    if (!autoInterface)
        return false;

    return autoInterface->m_wheelRatios[0] < 1.0f || autoInterface->m_wheelRatios[1] < 1.0f || autoInterface->m_wheelRatios[2] < 1.0f ||
           autoInterface->m_wheelRatios[3] < 1.0f;
}

// CAutomobile::HydraulicControl keeps m_wMiscComponentAngle at or above this while raised.
static constexpr std::uint16_t HYDRAULICS_RAISED_MISC_ANGLE = 500;

// What a lowering horn press leaves the angle at; the native control creeps it down to 0 from there.
static constexpr std::uint16_t HYDRAULICS_SETTLING_MISC_ANGLE = 60;

bool CAutomobileSA::IsHydraulicsRaised() const
{
    CAutomobileSAInterface* autoInterface = GetAutomobileInterface();
    if (!autoInterface)
        return false;

    return autoInterface->m_wMiscComponentAngle >= HYDRAULICS_RAISED_MISC_ANGLE;
}

// Writes the state HydraulicControl keys off; the suspension geometry follows on its next tick.
// Skips when the state already matches, leaving the native transition counters undisturbed.
void CAutomobileSA::SetHydraulicsRaised(bool bRaised)
{
    CAutomobileSAInterface* autoInterface = GetAutomobileInterface();
    if (!autoInterface)
        return;

    if ((autoInterface->m_wMiscComponentAngle >= HYDRAULICS_RAISED_MISC_ANGLE) == bRaised)
        return;

    autoInterface->m_wMiscComponentAngle = bRaised ? HYDRAULICS_RAISED_MISC_ANGLE : HYDRAULICS_SETTLING_MISC_ANGLE;
}

// Roughly a second of frames restocking the stance into the native control's one shot mailbox,
// enough for the chassis to settle onto the rebuilt geometry before the vehicle parks on it.
static constexpr unsigned char HYDRAULICS_STANCE_SETTLE_FRAMES = 50;

// Captured from the driver's own pad every driven frame by multiplayer_sa, or restocked from sync
// when the vehicle streams in; refreshing it re-arms the settle budget either way.
void CAutomobileSA::SetHydraulicsSuspensionStance(short sStickX, short sStickY, bool bShockButtonR)
{
    m_sHydraulicsStickX = sStickX;
    m_sHydraulicsStickY = sStickY;
    m_bHydraulicsShockButtonR = bShockButtonR;
    m_ucHydraulicsStanceSettleFrames = HYDRAULICS_STANCE_SETTLE_FRAMES;
}

void CAutomobileSAInterface::SetPanelDamage(std::uint8_t panelId, bool breakGlass, bool spawnFlyingComponent)
{
    int nodeId = CDamageManagerSA::GetCarNodeIndexFromPanel(panelId);
    if (nodeId < 0)
        return;

    eCarNodes node = static_cast<eCarNodes>(nodeId);

    RwFrame* frame = m_aCarNodes[nodeId];
    if (!frame)
        return;

    CVehicleModelInfoSAInterface* vehicleInfo = nullptr;
    if (auto* mi = pGame->GetModelInfo(m_nModelIndex))
        vehicleInfo = static_cast<CVehicleModelInfoSAInterface*>(mi->GetInterface());

    if (!vehicleInfo || !vehicleInfo->IsComponentDamageable(nodeId))
        return;

    switch (m_damageManager.GetPanelStatus(panelId))
    {
        case DT_PANEL_DAMAGED:
        {
            if ((pHandlingData->uiModelFlags & 0x10000000) != 0)  // check bouncePanels flag
                return;

            if (node != eCarNodes::WINDSCREEN && node != eCarNodes::WING_LF && node != eCarNodes::WING_RF)
            {
                // Get free bouncing panel
                for (auto& panel : m_panels)
                {
                    if (panel.m_nFrameId == (std::uint16_t)0xFFFF)
                    {
                        if (nodeId < 0 || nodeId > 0x7FFF)
                            return;

                        panel.SetPanel(static_cast<std::int16_t>(nodeId), static_cast<std::int16_t>(1), GetRandomNumberInRange(-0.2f, -0.5f));
                        break;
                    }
                }
            }

            SetComponentVisibility(frame, 2);  // ATOMIC_IS_DAM_STATE
            break;
        }
        case DT_PANEL_OPENED:
        {
            if (panelId == WINDSCREEN_PANEL)
                m_VehicleAudioEntity.AddAudioEvent(91, 0.0f);

            SetComponentVisibility(frame, 2);  // ATOMIC_IS_DAM_STATE
            break;
        }
        case DT_PANEL_OPENED_DAMAGED:
        {
            if (panelId == WINDSCREEN_PANEL)
            {
                if (breakGlass)
                    ((void(__cdecl*)(CAutomobileSAInterface*, bool))0x71C2B0)(this, false);  // Call CGlass::CarWindscreenShatters
            }

            if (spawnFlyingComponent && (panelId != WINDSCREEN_PANEL || (panelId == WINDSCREEN_PANEL && !breakGlass)))
                SpawnFlyingComponent(node, eCarComponentCollisionTypes::COL_NODE_PANEL);

            SetComponentVisibility(frame, 0);  // ATOMIC_IS_NOT_PRESENT
            break;
        }
    }
}
