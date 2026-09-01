/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CMultiplayerSA_Glass.cpp
 *  PURPOSE:     Shatter effect for script-owned glass panels
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/
#include "StdInc.h"

// CGlass::GeneratePanesForWindow, confirmed against gta-reversed's own tested hook signature for
// this address; a plain static utility function, not a real method, so it's a normal __cdecl call
// with the CVectors passed by value like any other C++ call, not a raw memory write into the
// engine's shard pool. Native windows call this with type 0 (default) or 1 (delayed); we always
// want the shards to appear immediately, so this only ever passes 0.
#define FUNC_CGlass_GeneratePanesForWindow 0x71B620

using GeneratePanesForWindowFn = void(__cdecl*)(int32 ePaneType, CVector point, CVector fwd, CVector right, CVector velocity, CVector center,
                                                 float velocityCenterDragCoeff, bool bShatter, bool numSectionsMax1, int32 numSections, bool unused);
static const auto GeneratePanesForWindow = reinterpret_cast<GeneratePanesForWindowFn>(FUNC_CGlass_GeneratePanesForWindow);

void CMultiplayerSA::ShatterGlassPanel(const CVector& vecCorner, const CVector& vecUp, const CVector& vecRight, const CVector& vecVelocity,
                                        const CVector& vecCenter, unsigned char ucGranularity)
{
    GeneratePanesForWindow(0, vecCorner, vecUp, vecRight, vecVelocity, vecCenter, 0.1f, true, false, std::max<unsigned char>(ucGranularity, 1), false);
}
