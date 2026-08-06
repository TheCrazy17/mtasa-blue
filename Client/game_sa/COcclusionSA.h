/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/COcclusionSA.h
 *  PURPOSE:     Occlusion zone implementation
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <game/COcclusion.h>
#include "COccluderSA.h"

class COcclusionSA final : public COcclusion
{
public:
    bool CreateZone(const SOcclusionZoneInfo& info, bool bInterior, std::size_t& outIndex) override;

    bool GetZoneData(std::size_t index, bool bInterior, SOcclusionZoneInfo& outInfo) override;
    bool SetZoneData(std::size_t index, bool bInterior, const SOcclusionZoneInfo& info) override;

    std::size_t GetZoneCount(bool bInterior) const override;

private:
    static SOccluderSAInterface* GetTable(bool bInterior);
    static std::uint32_t&        GetCount(bool bInterior);
    static std::size_t           GetMaxCount(bool bInterior);
};
