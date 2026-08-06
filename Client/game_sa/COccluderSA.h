/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/COccluderSA.h
 *  PURPOSE:     Occlusion zone (occluder) native data access
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <cstdint>

// Raw GTA:SA occluder entry, as loaded from map IPL data (occluder marker "occl").
// Everything is fixed-point to keep the on-disk/in-memory size small; positions and
// dimensions are stored as (value * 4), and rotation as a byte covering the full circle.
// Confirmed against the reversed engine (COccluder, sizeof 0x12) so map makers editing
// vanilla zones this way stay byte-compatible with what the game itself already loads.
struct SOccluderSAInterface
{
    std::int16_t  sCenterX;  // stored as (worldX * 4)
    std::int16_t  sCenterY;  // stored as (worldY * 4)
    std::int16_t  sCenterZ;  // stored as (worldZ * 4)
    std::int16_t  sLength;   // stored as (length * 4)
    std::int16_t  sWidth;    // stored as (width * 4)
    std::int16_t  sHeight;   // stored as (height * 4)
    std::uint8_t  ucRotZ;    // stored as (radians * 256 / (2*PI))
    std::uint8_t  ucRotY;
    std::uint8_t  ucRotX;
    std::uint16_t usNextIndexAndFlags;  // bit 0-14: next index in bucket list, bit 15: don't stream flag
};

static_assert(sizeof(SOccluderSAInterface) == 0x12, "SOccluderSAInterface must match the native COccluder layout");

// Native addresses for the two fixed-size occluder tables the game keeps in memory.
// Map occluders come from the world's IPL "occl" entries; interior occluders are the
// smaller, separate table used while inside an interior.
#define VAR_COcclusion_Occluders         0xC73FA0
#define VAR_COcclusion_NumOccludersOnMap 0xC73F98
#define MAX_MAP_OCCLUDERS                1000

#define VAR_COcclusion_InteriorOccluders    0xC73CC8
#define VAR_COcclusion_NumInteriorOccluders 0xC73CC4
#define MAX_INTERIOR_OCCLUDERS              40

// COcclusion::AddOne(float centerX, float centerY, float centerZ, float width, float length,
//                    float height, float rotZ, float rotY, float rotX, uint32 flags, bool isInterior)
// Rotation order is Z, Y, X, not X, Y, Z; matches the order the engine reads an "occl" IPL line in.
// Appends only, no bounds check against the table's own max size; callers must check
// NumOccludersOnMap/NumInteriorOccludersOnMap against MAX_MAP_OCCLUDERS/MAX_INTERIOR_OCCLUDERS first.
#define FUNC_COcclusion_AddOne 0x71DCD0
