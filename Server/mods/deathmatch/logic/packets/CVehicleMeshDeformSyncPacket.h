/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/packets/CVehicleMeshDeformSyncPacket.h
 *  PURPOSE:     Vehicle mesh deformation synchronization packet class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "CPacket.h"
#include <CVector.h>

class CVehicle;

// Carries the parameters of a single deformVehicle/stretchVehicleMesh/resetVehicleDeformation call,
// not raw vertex data - the receiving side replays the same deterministic algorithm locally instead
// of receiving a (potentially huge) list of vertex positions. Ordering matters: the dent ratchet/
// saturation logic is state-dependent on call order, so this is sent reliable+sequenced.
class CVehicleMeshDeformSyncPacket final : public CPacket
{
public:
    enum EMode : unsigned char
    {
        MODE_DENT = 0,
        MODE_STRETCH = 1,
        MODE_RESET = 2,            // whole-vehicle only - also clears the server's replay history
    };

    CVehicleMeshDeformSyncPacket() = default;

    ePacketID     GetPacketID() const { return PACKET_ID_VEHICLE_MESH_DEFORM_SYNC; };
    unsigned long GetFlags() const { return PACKET_HIGH_PRIORITY | PACKET_RELIABLE | PACKET_SEQUENCED; };

    bool Read(NetBitStreamInterface& BitStream);
    bool Write(NetBitStreamInterface& BitStream) const;

    ElementID     m_Vehicle = INVALID_ELEMENT_ID;
    unsigned char m_ucMode = MODE_DENT;

    // Dent/stretch only - unused (and not sent over the wire) for MODE_RESET
    CVector m_vecPoint;
    float   m_fRadius = 0.0f;
    float   m_fForce = 0.0f;             // dent only
    bool    m_bAffectWheels = false;             // dent only
    CVector m_vecDirection;              // stretch only
    float   m_fLength = 0.0f;            // stretch only
};
