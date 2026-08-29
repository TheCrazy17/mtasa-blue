/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/CAutomobile.h
 *  PURPOSE:     Automobile vehicle entity interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "Common.h"
#include "CVehicle.h"

class CPhysical;
class CDoor;

class CAutomobile : public virtual CVehicle
{
public:
    virtual ~CAutomobile() {};

    virtual bool IsAnyWheelTouchingGround() const = 0;
    virtual bool IsHydraulicsRaised() const = 0;
    virtual void SetHydraulicsRaised(bool bRaised) = 0;

    // The tilt a driver holds on the hydraulics stick; restocked from sync at stream in so
    // multiplayer_sa can reproduce the stance on a driverless vehicle.
    virtual void SetHydraulicsSuspensionStance(short sStickX, short sStickY, bool bShockButtonR) = 0;
};
