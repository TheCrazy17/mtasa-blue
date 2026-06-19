/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CWorldSA.WorldSizeExpansion.cpp
 *  PURPOSE:     Relocates CWorld::ms_aSectors and ms_aLodPtrLists to larger grids and raises
 *               the vanilla +-3000 unit world boundary used throughout the GTA:SA executable.
 *
 *  The list of patch addresses below is adapted from fastman92's "Limit Adjuster"
 *  (MIT licensed - https://github.com/fastman92/fastman92_limit_adjuster),
 *  MapLimits::PatchWorldSectors_GTA_SA_PC_1_0_HOODLUM(). Addresses target GTA:SA PC 1.0,
 *  the build MTA's game_sa module already addresses elsewhere (see ARRAY_StreamSectors_Original).
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CWorldSA.h"

DWORD g_ArrayStreamSectors = ARRAY_StreamSectors_Original;
int   g_NumStreamSectorRows = NUM_StreamSectorRows_Original;
int   g_NumStreamSectorCols = NUM_StreamSectorCols_Original;
float g_fWorldBoundary = WORLD_BOUND_Original;
DWORD g_ArrayStreamLodPtrLists = ARRAY_StreamLodPtrLists_Original;
int   g_NumStreamLodSectorRows = NUM_StreamLodSectorRows_Original;
int   g_NumStreamLodSectorCols = NUM_StreamLodSectorCols_Original;

// Targets of `fadd ds:[addr]` instructions for "half the number of LOD sectors per dimension" -
// same redirect-to-variable technique as s_fWorldBoundMin/Max below.
static float s_fNumLodSectorsPerDimensionHalf = static_cast<float>(NUM_StreamLodSectorRows_Original / 2);

// Targets of `fsub/fcomp/fld ds:[addr]` instructions - the absolute address embedded in the
// instruction is redirected to point at one of these variables, instead of the value being
// patched directly. Keeping them as variables (rather than re-patching per call) means the
// redirection only has to happen once, here, at ExpandWorldBoundary() time.
static float s_fWorldBoundMin = -WORLD_BOUND_Original;
static float s_fWorldBoundMax = WORLD_BOUND_Original;

void CWorldSA::ExpandWorldBoundary(float fNewBoundary)
{
    if (fNewBoundary <= WORLD_BOUND_Original)
        return;

    const int   numSectorsPerAxis = static_cast<int>((fNewBoundary * 2.0f) / WORLD_SECTOR_SIZE);
    const DWORD numTotalSectors = static_cast<DWORD>(numSectorsPerAxis) * static_cast<DWORD>(numSectorsPerAxis);

    // Each sector is { CPtrNodeSingleLink* buildingList; CPtrNodeSingleLink* dummyList; } (2 pointers).
    // Zero-initialised, which native code reads as an empty list (null head) - same as a freshly
    // booted game before anything streams in.
    auto*       pNewSectors = new DWORD[static_cast<size_t>(numTotalSectors) * 2]();
    const DWORD dwNewArray = reinterpret_cast<DWORD>(pNewSectors);
    const DWORD dwNewDummyListBase = dwNewArray + 4;

    g_ArrayStreamSectors = dwNewArray;
    g_NumStreamSectorRows = numSectorsPerAxis;
    g_NumStreamSectorCols = numSectorsPerAxis;
    g_fWorldBoundary = fNewBoundary;

    s_fWorldBoundMin = -fNewBoundary;
    s_fWorldBoundMax = fNewBoundary;

    // --- Redirect every instruction referencing the old, fixed-size ms_aSectors array ---
    static constexpr DWORD buildingListPatchPoints[] = {
        0x408258 + 1, 0x4086FF + 3, 0x408706 + 3, 0x40914E + 3, 0x4092E9 + 3, 0x40D68C + 3, 0x40D98C + 3, 0x40DB0E + 3,
        0x40DC29 + 3, 0x40DDCE + 3, 0x40DF5E + 3, 0x40E061 + 3, 0x41A85A + 3, 0x41A861 + 3, 0x534A09 + 3, 0x534D6B + 3,
        0x546738 + 3, 0x54BB23 + 3, 0x563844 + 1, 0x564171 + 1, 0x56421F + 1, 0x56479D + 3, 0x564B59 + 3, 0x564DA9 + 3,
        0x5664D7 + 3, 0x566586 + 3, 0x5675FA + 3, 0x567735 + 3, 0x568CB9 + 3, 0x568EFB + 3, 0x569127 + 3, 0x569537 + 3,
        0x569F57 + 3, 0x56A685 + 3, 0x56A766 + 3, 0x56A85F + 3, 0x56A940 + 3, 0x56ACA3 + 3, 0x56AD91 + 3, 0x56AE95 + 3,
        0x56AF74 + 3, 0x56B2C3 + 3, 0x56B3B1 + 3, 0x56B4B5 + 3, 0x56B594 + 3, 0x56BC53 + 3, 0x56BD46 + 3, 0x56BE56 + 3,
        0x56BF4F + 3, 0x56C341 + 3, 0x56C445 + 3, 0x56C569 + 3, 0x56C664 + 3, 0x56CA4A + 3, 0x56CB51 + 3, 0x56CC79 + 3,
        0x56CD84 + 3, 0x5DC7A7 + 3, 0x6063C7 + 3, 0x67FF5D + 3, 0x699CF5 + 3, 0x699CFE + 3, 0x6E31AA + 3, 0x70ACB2 + 3,
        0x70B9BA + 3, 0x711B2F + 3, 0x84E9C1 + 1, 0x85652C + 1, 0x1566855 + 3,
    };
    for (DWORD addr : buildingListPatchPoints)
        MemPut<DWORD>(addr, dwNewArray);

    static constexpr DWORD dummyListPatchPoints[] = {
        0x4091C5 + 3, 0x409367 + 3, 0x40D9C5 + 3, 0x40DB47 + 3, 0x40DC61 + 3, 0x40DE07 + 3, 0x40DF97 + 3, 0x40E09A + 3,
        0x534A98 + 3, 0x534DFA + 3, 0x71CDB0 + 3,
    };
    for (DWORD addr : dummyListPatchPoints)
        MemPut<DWORD>(addr, dwNewDummyListBase);

    // "End of ms_aSectors" comparisons, used to detect the start of ms_aRepeatSectors while iterating
    const DWORD dwNewArrayEnd = dwNewArray + numTotalSectors * 8;
    MemPut<DWORD>(0x5634A4 + 2, dwNewArrayEnd);
    MemPut<DWORD>(0x5638DD + 2, dwNewArrayEnd);
    MemPut<DWORD>(0x56420D + 2, dwNewArrayEnd);
    MemPut<DWORD>(0x564281 + 2, dwNewArrayEnd);

    // Sector count used when iterating the whole grid (originally 14400 = 120*120)
    MemPut<DWORD>(0x84E9BA + 1, numTotalSectors);
    MemPut<DWORD>(0x856525 + 1, numTotalSectors);

    // --- Redirect ds:[addr] operands so the FPU reads our variables instead of the vanilla constants ---
    static constexpr DWORD minCoordOperandPoints[] = {
        0x47F0E6 + 2, 0x47F17B + 2, 0x5347EB + 2, 0x534832 + 2, 0x534AFD + 2, 0x534B2A + 2, 0x534B53 + 2,
        0x5615DF + 2, 0x5615EE + 2, 0x561627 + 2, 0x6C660F + 2, 0x6C6631 + 2, 0x7361B0 + 2, 0x7361DE + 2,
    };
    for (DWORD addr : minCoordOperandPoints)
        MemPut<DWORD>(addr, reinterpret_cast<DWORD>(&s_fWorldBoundMin));

    static constexpr DWORD maxCoordOperandPoints[] = {
        0x534819 + 2, 0x53484B + 2, 0x534B3A + 2, 0x534B6C + 2, 0x561604 + 2, 0x56161A + 2, 0x56163B + 2,
        0x561645 + 2, 0x561659 + 2, 0x6C6620 + 2, 0x6C6642 + 2, 0x70E416 + 2, 0x7361C9 + 2, 0x7361F3 + 2,
    };
    for (DWORD addr : maxCoordOperandPoints)
        MemPut<DWORD>(addr, reinterpret_cast<DWORD>(&s_fWorldBoundMax));

    // --- Immediate float values embedded directly in `mov [stack], imm32` instructions ---
    const float fMin = -fNewBoundary;
    const float fMax = fNewBoundary;
    const float fMaxMinusOne = fNewBoundary - 1.0f;

    static constexpr DWORD minCoordImmPoints[] = {
        0x405EE2 + 4, 0x405EEA + 4, 0x41140F + 4, 0x411417 + 4, 0x53480D + 4, 0x53483F + 4, 0x534B60 + 4,
        0x156A70B + 4, 0x156A713 + 4,
    };
    for (DWORD addr : minCoordImmPoints)
        MemPut<float>(addr, fMin);

    static constexpr DWORD maxCoordImmPoints[] = {
        0x405EF2 + 4, 0x405EFA + 4, 0x41141F + 4, 0x411427 + 4, 0x156A71B + 4, 0x156A723 + 4,
    };
    for (DWORD addr : maxCoordImmPoints)
        MemPut<float>(addr, fMax);

    static constexpr DWORD maxMinusOneImmPoints[] = {
        0x534826 + 4,
        0x534858 + 4,
        0x534B47 + 4,
        0x534B79 + 4,
    };
    for (DWORD addr : maxMinusOneImmPoints)
        MemPut<float>(addr, fMaxMinusOne);

    // --- ms_aLodPtrLists ("big building" list used by CRenderer::ScanBigBuildingList) ---
    // Cell size (200 units) is left unchanged, same reasoning as the main sector grid above.
    const int   numLodSectorsPerAxis = static_cast<int>((fNewBoundary * 2.0f) / WORLD_LOD_SECTOR_SIZE);
    const DWORD numTotalLodSectors = static_cast<DWORD>(numLodSectorsPerAxis) * static_cast<DWORD>(numLodSectorsPerAxis);

    // Each slot is a single CPtrListSingleLink<CEntity*> head pointer.
    auto*       pNewLodPtrLists = new DWORD[numTotalLodSectors]();
    const DWORD dwNewLodArray = reinterpret_cast<DWORD>(pNewLodPtrLists);

    g_ArrayStreamLodPtrLists = dwNewLodArray;
    g_NumStreamLodSectorRows = numLodSectorsPerAxis;
    g_NumStreamLodSectorCols = numLodSectorsPerAxis;
    s_fNumLodSectorsPerDimensionHalf = static_cast<float>(numLodSectorsPerAxis / 2);

    static constexpr DWORD lodPtrListPatchPoints[] = {
        0x4072CD + 3, 0x40C61A + 3, 0x5348DA + 3, 0x534BF8 + 3, 0x554B54 + 3,
        0x56405A + 1, 0x564F87 + 3, 0x84EA21 + 1, 0x8564CC + 1,
    };
    for (DWORD addr : lodPtrListPatchPoints)
        MemPut<DWORD>(addr, dwNewLodArray);

    // "End of ms_aLodPtrLists" comparison (originally pointed at ms_listMovingEntityPtrs)
    MemPut<DWORD>(0x56409C + 2, dwNewLodArray + numTotalLodSectors * 4);

    // Sector count used when iterating the whole LOD grid (originally 900 = 30*30)
    MemPut<DWORD>(0x84EA1A + 1, numTotalLodSectors);
    MemPut<DWORD>(0x8564C5 + 1, numTotalLodSectors);

    // Half the number of LOD sectors per dimension (redirected ds:[addr] operand, originally 15.0)
    static constexpr DWORD lodHalfCountOperandPoints[] = {
        0x40C568 + 2, 0x40C584 + 2, 0x40C5A7 + 2, 0x40C5D2 + 2, 0x534876 + 2, 0x53488D + 2,
        0x5348A6 + 2, 0x5348BD + 2, 0x534B93 + 2, 0x534BAA + 2, 0x534BC5 + 2, 0x534BDC + 2,
        0x55556D + 2, 0x555584 + 2, 0x5555AA + 2, 0x5555C1 + 2, 0x5555D8 + 2, 0x5555EF + 2,
        0x555609 + 2, 0x555623 + 2, 0x55563D + 2, 0x555657 + 2, 0x564EE7 + 2, 0x564F03 + 2,
        0x564F1C + 2, 0x564F3A + 2,
    };
    for (DWORD addr : lodHalfCountOperandPoints)
        MemPut<DWORD>(addr, reinterpret_cast<DWORD>(&s_fNumLodSectorsPerDimensionHalf));
}
