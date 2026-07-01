/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/packets/CVehicleMeshDeformSyncPacket.cpp
 *  PURPOSE:     Vehicle mesh deformation synchronization packet class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CVehicleMeshDeformSyncPacket.h"

bool CVehicleMeshDeformSyncPacket::Read(NetBitStreamInterface& BitStream)
{
    if (!BitStream.Read(m_Vehicle) || !BitStream.ReadBits(&m_ucMode, 2))
        return false;

    if (m_ucMode == MODE_RESET)
        return true;

    if (!BitStream.Read(m_vecPoint.fX) || !BitStream.Read(m_vecPoint.fY) || !BitStream.Read(m_vecPoint.fZ) || !BitStream.Read(m_fRadius))
        return false;

    if (m_ucMode == MODE_STRETCH)
    {
        return BitStream.Read(m_vecDirection.fX) && BitStream.Read(m_vecDirection.fY) && BitStream.Read(m_vecDirection.fZ) && BitStream.Read(m_fLength);
    }
    return BitStream.Read(m_fForce) && BitStream.ReadBit(m_bAffectWheels);
}

bool CVehicleMeshDeformSyncPacket::Write(NetBitStreamInterface& BitStream) const
{
    BitStream.Write(m_Vehicle);
    BitStream.WriteBits(&m_ucMode, 2);

    if (m_ucMode == MODE_RESET)
        return true;

    BitStream.Write(m_vecPoint.fX);
    BitStream.Write(m_vecPoint.fY);
    BitStream.Write(m_vecPoint.fZ);
    BitStream.Write(m_fRadius);

    if (m_ucMode == MODE_STRETCH)
    {
        BitStream.Write(m_vecDirection.fX);
        BitStream.Write(m_vecDirection.fY);
        BitStream.Write(m_vecDirection.fZ);
        BitStream.Write(m_fLength);
    }
    else
    {
        BitStream.Write(m_fForce);
        BitStream.WriteBit(m_bAffectWheels);
    }

    return true;
}
