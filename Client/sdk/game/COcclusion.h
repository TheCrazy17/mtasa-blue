/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/COcclusion.h
 *  PURPOSE:     Occlusion zone interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once
#include <CVector.h>

// Position/size in world units, rotation in degrees; matches the convention every other
// MTA position/rotation getter/setter already uses, not the native fixed-point encoding.
struct SOcclusionZoneInfo
{
    CVector vecPosition;
    CVector vecSize;
    CVector vecRotation;
};

class COcclusion
{
public:
    virtual bool CreateZone(const SOcclusionZoneInfo& info, bool bInterior, std::size_t& outIndex) = 0;

    virtual bool GetZoneData(std::size_t index, bool bInterior, SOcclusionZoneInfo& outInfo) = 0;
    virtual bool SetZoneData(std::size_t index, bool bInterior, const SOcclusionZoneInfo& info) = 0;

    virtual std::size_t GetZoneCount(bool bInterior) const = 0;
};
