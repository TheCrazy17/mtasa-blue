/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CCameraSA.cpp
 *  PURPOSE:     Camera rendering
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CCameraSA.h"
#include "CGameSA.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 6.28318530717958647692f;

    inline float WrapAngleRad(float angle) noexcept
    {
        // Wrap into [-pi, pi] using one multiplication and floor
        angle -= kTwoPi * std::floor((angle + kPi) / kTwoPi);
        if (angle <= -kPi)
            angle += kTwoPi;
        else if (angle > kPi)
            angle -= kTwoPi;
        return angle;
    }

    inline bool IsFiniteVector(const CVector& vec) noexcept
    {
        return std::isfinite(vec.fX) && std::isfinite(vec.fY) && std::isfinite(vec.fZ);
    }

    // Locks pTexture's top surface and hand-writes it as an uncompressed 24bpp BMP to filePath.
    // Only D3DFMT_A8R8G8B8/X8R8G8B8/R5G6B5 are understood; anything else fails. Written this way
    // (rather than via D3DXSaveTextureToFile) because Game SA doesn't link d3dx9.lib - only Client
    // Core and Client GUI do - and this is debug/test-only code, so a hand-rolled BMP writer is
    // simpler than adding that dependency just to validate RenderWorldToRaster works.
    bool SaveTextureAsBmp(IDirect3DTexture9* pTexture, const char* filePath)
    {
        if (!pTexture || !filePath)
            return false;

        IDirect3DSurface9* pSurface = nullptr;
        if (FAILED(pTexture->GetSurfaceLevel(0, &pSurface)))
            return false;

        D3DSURFACE_DESC desc;
        pSurface->GetDesc(&desc);

        D3DLOCKED_RECT lockedRect;
        if (FAILED(pSurface->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY)))
        {
            pSurface->Release();
            return false;
        }

        const int width = static_cast<int>(desc.Width);
        const int height = static_cast<int>(desc.Height);
        const int rowSize = ((width * 3 + 3) / 4) * 4;  // rows are padded to a 4-byte boundary

        std::vector<std::uint8_t> pixelData(static_cast<std::size_t>(rowSize) * height);
        const auto*               pSrcBase = static_cast<const std::uint8_t*>(lockedRect.pBits);
        bool                      formatSupported = true;

        for (int y = 0; y < height && formatSupported; ++y)
        {
            const std::uint8_t* pSrcRow = pSrcBase + static_cast<std::size_t>(y) * lockedRect.Pitch;
            // BMP rows are stored bottom-up
            std::uint8_t* pDstRow = pixelData.data() + static_cast<std::size_t>(height - 1 - y) * rowSize;

            for (int x = 0; x < width; ++x)
            {
                std::uint8_t b, g, r;
                if (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8)
                {
                    const std::uint8_t* pPixel = pSrcRow + static_cast<std::size_t>(x) * 4;
                    b = pPixel[0];
                    g = pPixel[1];
                    r = pPixel[2];
                }
                else if (desc.Format == D3DFMT_R5G6B5)
                {
                    const auto pixel16 = *reinterpret_cast<const std::uint16_t*>(pSrcRow + static_cast<std::size_t>(x) * 2);
                    r = static_cast<std::uint8_t>(((pixel16 >> 11) & 0x1F) << 3);
                    g = static_cast<std::uint8_t>(((pixel16 >> 5) & 0x3F) << 2);
                    b = static_cast<std::uint8_t>((pixel16 & 0x1F) << 3);
                }
                else
                {
                    formatSupported = false;
                    break;
                }

                std::uint8_t* pDstPixel = pDstRow + static_cast<std::size_t>(x) * 3;
                pDstPixel[0] = b;
                pDstPixel[1] = g;
                pDstPixel[2] = r;
            }
        }

        pSurface->UnlockRect();
        pSurface->Release();

        if (!formatSupported)
            return false;

        BITMAPFILEHEADER fileHeader{};
        fileHeader.bfType = 0x4D42;  // 'BM'
        fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        fileHeader.bfSize = fileHeader.bfOffBits + static_cast<DWORD>(pixelData.size());

        BITMAPINFOHEADER infoHeader{};
        infoHeader.biSize = sizeof(BITMAPINFOHEADER);
        infoHeader.biWidth = width;
        infoHeader.biHeight = height;
        infoHeader.biPlanes = 1;
        infoHeader.biBitCount = 24;
        infoHeader.biCompression = BI_RGB;
        infoHeader.biSizeImage = static_cast<DWORD>(pixelData.size());

        FILE* pFile = fopen(filePath, "wb");
        if (!pFile)
            return false;

        fwrite(&fileHeader, sizeof(fileHeader), 1, pFile);
        fwrite(&infoHeader, sizeof(infoHeader), 1, pFile);
        fwrite(pixelData.data(), pixelData.size(), 1, pFile);
        fclose(pFile);

        return true;
    }
}

extern CGameSA* pGame;

enum class CameraClipFlags : uint8_t
{
    Objects = 1u << 0,
    Vehicles = 1u << 1,
};

static std::atomic<uint8_t> s_cameraClipMask{static_cast<uint8_t>(CameraClipFlags::Objects) | static_cast<uint8_t>(CameraClipFlags::Vehicles)};

#define VAR_CameraClipVehicles       0x8A5B14
#define VAR_CameraClipDynamicObjects 0x8A5B15
#define VAR_CameraClipStaticObjects  0x8A5B16
#define VAR_RelVelCamCollisionVehSqr 0x8A5B18

#define HOOKPOS_Camera_CollisionDetection 0x520190
DWORD RETURN_Camera_CollisionDetection = 0x520195;
void  HOOK_Camera_CollisionDetection();

CCameraSA::CCameraSA(CCameraSAInterface* cameraInterface)
{
    if (!cameraInterface)
    {
        internalInterface = nullptr;
        // Initialize all camera pointers to null
        for (int i = 0; i < MAX_CAMS; i++)
            Cams[i] = nullptr;
        return;
    }

    internalInterface = cameraInterface;

    for (int i = 0; i < MAX_CAMS; i++)
    {
        try
        {
            Cams[i] = new CCamSA(&internalInterface->Cams[i]);
        }
        catch (...)
        {
            // Clean up on failure
            for (int j = 0; j < i; j++)
            {
                delete Cams[j];
                Cams[j] = nullptr;
            }
            internalInterface = nullptr;
            throw;
        }
    }

    s_cameraClipMask.store(static_cast<uint8_t>(CameraClipFlags::Objects) | static_cast<uint8_t>(CameraClipFlags::Vehicles), std::memory_order_relaxed);

    HookInstall(HOOKPOS_Camera_CollisionDetection, (DWORD)HOOK_Camera_CollisionDetection, 5);
}

CCameraSA::~CCameraSA()
{
    for (int i = 0; i < MAX_CAMS; i++)
    {
        if (Cams[i])
        {
            delete Cams[i];
            Cams[i] = nullptr;
        }
    }
}

void CCameraSA::Restore()
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;

    DWORD dwFunc = FUNC_Restore;
    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        call    dwFunc
    }
    // clang-format on
}

void CCameraSA::RestoreWithJumpCut()
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;
    DWORD dwFunc = 0x50BD40;
    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        call    dwFunc
    }
    // clang-format on
    dwFunc = 0x50BAB0;
    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        call    dwFunc
    }
    // clang-format on
}

/**
 * \todo Find out what the last two parameters are
 */
void CCameraSA::TakeControl(CEntity* entity, eCamMode CamMode, int CamSwitchStyle)
{
    if (!entity)
        return;

    CEntitySA* pEntitySA = dynamic_cast<CEntitySA*>(entity);
    if (!pEntitySA)
        return;

    CEntitySAInterface* entityInterface = pEntitySA->GetInterface();
    if (!entityInterface)
        return;

    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;

    if (CamSwitchStyle < 0)
        CamSwitchStyle = 0;
    else if (CamSwitchStyle > 10)
        CamSwitchStyle = 10;

    DWORD CCamera__TakeControl = FUNC_TakeControl;
    // clang-format off
    __asm
    {
        mov ecx, cameraInterface
        push 1
        push CamSwitchStyle
        push CamMode
        push entityInterface
        call CCamera__TakeControl
    }
    // clang-format on
}

void CCameraSA::TakeControl(CVector* position, int CamSwitchStyle)
{
    if (!position)
        return;

    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;

    if (CamSwitchStyle < 0)
        CamSwitchStyle = 0;
    else if (CamSwitchStyle > 10)
        CamSwitchStyle = 10;
    // __thiscall
    CVector vecOffset;
    /*  vecOffset.fZ = 0.5f;
        vecOffset.fY = 0.5f;
        vecOffset.fX = 0.5f;*/
    /*  DWORD dwFunc = 0x50BEC0;
        // clang-format off
        __asm
        {
            mov ecx, cameraInterface
            lea     eax, vecOffset
            push    eax
            push    position
            call    dwFunc
        }*/
    // clang-format on

    DWORD CCamera__TakeControlNoEntity = FUNC_TakeControlNoEntity;
    // clang-format off
    __asm
        {
        mov ecx, cameraInterface
        push 1
        push CamSwitchStyle
        push position
        call CCamera__TakeControlNoEntity
        }
    // clang-format on

    DWORD dwFunc = 0x50BEC0;
    // clang-format off
    __asm
    {
        mov ecx, cameraInterface
        lea     eax, vecOffset
        push    eax
        push    position
        call    dwFunc
    }
    // clang-format on
}

// LSOD recovery
void CCameraSA::RestoreLastGoodState()
{
    CMatrix defmat;
    SetMatrix(&defmat);

    CCameraSAInterface* pCameraInterface = GetInterface();

    if (!pCameraInterface)
        return;

    pCameraInterface->m_CameraAverageSpeed = 0;
    pCameraInterface->m_CameraSpeedSoFar = 0;
    pCameraInterface->m_PreviousCameraPosition = CVector();
    pCameraInterface->m_vecGameCamPos = CVector();
    pCameraInterface->m_cameraMatrix.SetFromMatrixSkipPadding(CMatrix());
    pCameraInterface->m_cameraMatrixOld.SetFromMatrixSkipPadding(CMatrix());
    pCameraInterface->m_viewMatrix.SetFromMatrixSkipPadding(CMatrix());
    pCameraInterface->m_matInverse.SetFromMatrixSkipPadding(CMatrix());
    pCameraInterface->m_vecBottomFrustumNormal = CVector(0, -1, -1);
    pCameraInterface->m_vecTopFrustumNormal = CVector(-1, -1, 0);

    for (uint i = 0; i < MAX_CAMS; i++)
    {
        CCamSA* pCam = Cams[i];
        if (!pCam)
            continue;

        CCamSAInterface* pCamInterface = pCam->GetInterface();
        if (!pCamInterface)
            continue;

        pCamInterface->m_fAlphaSpeedOverOneFrame = 0;
        pCamInterface->m_fBetaSpeedOverOneFrame = 0;
        pCamInterface->m_fTrueBeta = 1;
        pCamInterface->m_fTrueAlpha = 1;
        pCamInterface->m_fVerticalAngle = 1;
        pCamInterface->m_fHorizontalAngle = 1;
        pCamInterface->BetaSpeed = 0;
        pCamInterface->SpeedVar = 0;

        pCamInterface->m_cvecSourceSpeedOverOneFrame = CVector(0, 0, 0);
        pCamInterface->m_cvecTargetSpeedOverOneFrame = CVector(0, 0, 0);
        pCamInterface->Front = CVector(1, 0, 0);
        pCamInterface->Source = CVector(1, 0, 0);
        pCamInterface->SourceBeforeLookBehind = CVector(1, 0, 0);
        pCamInterface->Up = CVector(0, 0, 1);
        for (uint i = 0; i < CAM_NUM_TARGET_HISTORY; i++)
            pCamInterface->m_aTargetHistoryPos[i] = CVector(1, 0, 0);
    }
}

CMatrix* CCameraSA::GetMatrix(CMatrix* matrix)
{
    if (!matrix)
        return nullptr;

    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
    {
        *matrix = CMatrix();
        return matrix;
    }

    CMatrix_Padded* pCamMatrix = &cameraInterface->m_cameraMatrix;
    if (pCamMatrix)
    {
        matrix->vFront = pCamMatrix->vFront;
        matrix->vPos = pCamMatrix->vPos;
        matrix->vUp = pCamMatrix->vUp;
        matrix->vRight = pCamMatrix->vRight;

        if (!IsValidMatrix(*matrix))
        {
            RestoreLastGoodState();
            pCamMatrix->ConvertToMatrix(*matrix);
        }
    }
    else
    {
        *matrix = CMatrix();
    }
    return matrix;
}

void CCameraSA::SetMatrix(CMatrix* matrix)
{
    if (!matrix)
        return;

    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;

    CMatrix_Padded* pCamMatrix = &cameraInterface->m_cameraMatrix;
    if (pCamMatrix)
    {
        pCamMatrix->vFront = matrix->vFront;
        pCamMatrix->vPos = matrix->vPos;
        pCamMatrix->vUp = matrix->vUp;
        pCamMatrix->vRight = matrix->vRight;
    }
}

void CCameraSA::Find3rdPersonCamTargetVector(float fDistance, CVector* vecGunMuzzle, CVector* vecSource, CVector* vecTarget)
{
    if (!vecGunMuzzle || !vecSource || !vecTarget)
        return;

    // Validate float parameter to prevent NaN/infinity issues
    if (!std::isfinite(fDistance) || fDistance < 0.0f)
        return;

    float fOriginX = vecGunMuzzle->fX;
    float fOriginY = vecGunMuzzle->fY;
    float fOriginZ = vecGunMuzzle->fZ;

    if (!std::isfinite(fOriginX) || !std::isfinite(fOriginY) || !std::isfinite(fOriginZ))
        return;

    DWORD               dwFunc = FUNC_Find3rdPersonCamTargetVector;
    CCameraSAInterface* cameraInterface = GetInterface();

    if (!cameraInterface)
        return;

    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        push    vecTarget
        push    vecSource
        push    fOriginZ
        push    fOriginY
        push    fOriginX
        push    fDistance
        call    dwFunc
    }
    // clang-format on
}

float CCameraSA::Find3rdPersonQuickAimPitch()
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return 0.0f;

    float fReturn;
    DWORD dwFunc = FUNC_Find3rdPersonQuickAimPitch;
    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        call    dwFunc
        fstp    fReturn
    }
    // clang-format on
    return fReturn;
}

BYTE CCameraSA::GetActiveCam()
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return 0;
    return cameraInterface->ActiveCam;
}

CCam* CCameraSA::GetCam(BYTE bCameraID)
{
    if (bCameraID < MAX_CAMS)
        return Cams[bCameraID];

    return NULL;
}

CCam* CCameraSA::GetCam(CCamSAInterface* camInterface)
{
    for (int i = 0; i < MAX_CAMS; i++)
    {
        if (Cams[i] && Cams[i]->GetInterface() == camInterface)
        {
            return Cams[i];
        }
    }

    return NULL;
}

void CCameraSA::SetWidescreen(bool bWidescreen)
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;
    cameraInterface->m_WideScreenOn = bWidescreen;
}

bool CCameraSA::GetWidescreen()
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return false;
    return cameraInterface->m_WideScreenOn;
}

bool CCameraSA::IsFading()
{
    DWORD               dwFunc = FUNC_GetFading;
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return false;
    bool bRet = false;
    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        call    dwFunc
        mov     bRet, al
    }
    // clang-format on
    return bRet;
}

int CCameraSA::GetFadingDirection()
{
    DWORD               dwFunc = FUNC_GetFadingDirection;
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return 0;
    int dwRet = false;
    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        call    dwFunc
        mov     dwRet, eax
    }
    // clang-format on
    return dwRet;
}

void CCameraSA::Fade(float fFadeOutTime, int iOutOrIn)
{
    if (!std::isfinite(fFadeOutTime))
        return;

    if (fFadeOutTime < 0.0f)
        fFadeOutTime = 0.0f;
    else if (fFadeOutTime > 60.0f)
        fFadeOutTime = 60.0f;

    if (iOutOrIn < 0)
        iOutOrIn = 0;
    else if (iOutOrIn > 1)
        iOutOrIn = 1;

    DWORD               dwFunc = FUNC_Fade;
    CCameraSAInterface* cameraInterface = GetInterface();

    if (!cameraInterface)
        return;

    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        push    iOutOrIn
        push    fFadeOutTime
        call    dwFunc
    }
    // clang-format on
}

void CCameraSA::SetFadeColor(unsigned char ucRed, unsigned char ucGreen, unsigned char ucBlue)
{
    DWORD               dwFunc = FUNC_SetFadeColour;
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;
    DWORD dwRed = ucRed;
    DWORD dwGreen = ucGreen;
    DWORD dwBlue = ucBlue;
    // clang-format off
    __asm
    {
        mov     ecx, cameraInterface
        push    dwBlue
        push    dwGreen
        push    dwRed
        call    dwFunc
    }
    // clang-format on
}

float CCameraSA::GetCameraRotation()
{
    return *(float*)VAR_CameraRotation;
}

RwMatrix* CCameraSA::GetLTM()
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return nullptr;

    if (!cameraInterface->m_pRwCamera)
        return nullptr;

    if (!cameraInterface->m_pRwCamera->object.object.parent)
        return nullptr;

    // RwFrameGetLTM
    return ((RwMatrix * (_cdecl*)(void*))0x7F0990)(cameraInterface->m_pRwCamera->object.object.parent);
}

CEntity* CCameraSA::GetTargetEntity()
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return nullptr;

    if (!pGame)
        return nullptr;

    CEntitySAInterface* pInterface = cameraInterface->pTargetEntity;
    if (pInterface)
    {
        CPools* pPools = pGame->GetPools();
        if (!pPools)
            return nullptr;

        return pPools->GetEntity((DWORD*)pInterface);
    }
    return nullptr;
}

void CCameraSA::SetCameraClip(bool bObjects, bool bVehicles)
{
    uint8_t newMask = 0;
    if (bObjects)
        newMask |= static_cast<uint8_t>(CameraClipFlags::Objects);
    if (bVehicles)
        newMask |= static_cast<uint8_t>(CameraClipFlags::Vehicles);
    s_cameraClipMask.store(newMask, std::memory_order_relaxed);
}

void CCameraSA::ResetCameraClip()
{
    s_cameraClipMask.store(static_cast<uint8_t>(CameraClipFlags::Objects) | static_cast<uint8_t>(CameraClipFlags::Vehicles), std::memory_order_relaxed);
}

void CCameraSA::GetCameraClip(bool& bObjects, bool& bVehicles)
{
    const uint8_t mask = s_cameraClipMask.load(std::memory_order_relaxed);
    bObjects = (mask & static_cast<uint8_t>(CameraClipFlags::Objects)) != 0;
    bVehicles = (mask & static_cast<uint8_t>(CameraClipFlags::Vehicles)) != 0;
}

// At speed, relax camera collision against dynamic (script-created) objects only.
// Static world geometry always keeps collision so default GTA world/buildings still block the camera.
// When the camera target is another player's vehicle, use that vehicle's speed rather than the local player
static void ApplyVehicleSpeedCameraClip()
{
    // Static-world clip stays on regardless of speed.
    MemPutFast<char>(VAR_CameraClipStaticObjects, 1);

    using FindPlayerVehicle_t = void*(__cdecl*)(int playerId, bool bIncludeRemote);
    auto FindPlayerVehicle = reinterpret_cast<FindPlayerVehicle_t>(0x56E0D0);

    void* pVehicle = nullptr;

    // Check the camera's actual target entity first: when spectating another player who is driving,
    // the camera target is their vehicle, not the local player's.
    CCamera* pCamera = pGame ? pGame->GetCamera() : nullptr;
    if (pCamera)
    {
        CEntity* pTargetEntity = pCamera->GetTargetEntity();
        if (pTargetEntity && pTargetEntity->GetEntityType() == ENTITY_TYPE_VEHICLE)
        {
            pVehicle = pTargetEntity->GetInterface();
        }
    }

    // Fall back to local player's vehicle if the camera isn't targeting a vehicle.
    if (!pVehicle)
    {
        pVehicle = FindPlayerVehicle(-1, false);
    }

    // No vehicle to derive speed from: restore stock defaults so the camera collides with everything.
    if (!pVehicle)
    {
        MemPutFast<float>(VAR_RelVelCamCollisionVehSqr, 1.0f);
        MemPutFast<char>(VAR_CameraClipDynamicObjects, 1);
        return;
    }

    // Apply camera clipping for dynamic objects
    // CPhysicalSAInterface::m_vecLinearVelocity at offset 0x44 (CVector: 3 floats)
    float* pSpeed = reinterpret_cast<float*>(static_cast<char*>(pVehicle) + 0x44);
    float  speedSq = pSpeed[0] * pSpeed[0] + pSpeed[1] * pSpeed[1] + pSpeed[2] * pSpeed[2];
    bool   slow = speedSq <= (0.2f * 0.2f);

    MemPutFast<float>(VAR_RelVelCamCollisionVehSqr, slow ? 0.1f : 1.0f);
    MemPutFast<char>(VAR_CameraClipDynamicObjects, slow ? 1 : 0);
}

static void _cdecl DoCameraCollisionDetectionPokes()
{
    const uint8_t mask = s_cameraClipMask.load(std::memory_order_relaxed);

    // Objects clip on = GTA default (speed-dependent dynamic, always-on static); off = force off.
    if (mask & static_cast<uint8_t>(CameraClipFlags::Objects))
        ApplyVehicleSpeedCameraClip();
    else
    {
        MemPutFast<char>(VAR_CameraClipDynamicObjects, 0);
        MemPutFast<char>(VAR_CameraClipStaticObjects, 0);
    }

    // Vehicles clip on = GTA default (always on); off = force off.
    if (mask & static_cast<uint8_t>(CameraClipFlags::Vehicles))
        MemPutFast<char>(VAR_CameraClipVehicles, 1);
    else
        MemPutFast<char>(VAR_CameraClipVehicles, 0);
}

static void __declspec(naked) HOOK_Camera_CollisionDetection()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        pushad
        call    DoCameraCollisionDetectionPokes
        popad

        sub     esp, 24h
        push    ebx
        push    ebp
        jmp     RETURN_Camera_CollisionDetection
    }
    // clang-format on
}

BYTE CCameraSA::GetCameraVehicleViewMode()
{
    return *(BYTE*)VAR_VehicleCameraView;
}

BYTE CCameraSA::GetCameraPedViewMode()
{
    return *(BYTE*)VAR_PedCameraView;
}

void CCameraSA::SetCameraVehicleViewMode(BYTE dwCamMode)
{
    MemPutFast<BYTE>(VAR_VehicleCameraView, dwCamMode);
}

void CCameraSA::SetCameraPedViewMode(BYTE dwCamMode)
{
    MemPutFast<BYTE>(VAR_PedCameraView, dwCamMode);
}

void CCameraSA::SetShakeForce(float fShakeForce)
{
    CCameraSAInterface* pCameraInterface = GetInterface();
    if (!pCameraInterface)
        return;
    pCameraInterface->m_fCamShakeForce = fShakeForce;
}

float CCameraSA::GetShakeForce()
{
    CCameraSAInterface* pCameraInterface = GetInterface();
    if (!pCameraInterface)
        return 0.0f;
    return pCameraInterface->m_fCamShakeForce;
}

void CCameraSA::ShakeCamera(float radius, float x, float y, float z) noexcept
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;
    if (radius <= 0.0f)
        return ResetShakeCamera();

    using ShakeCamera_t = void(__thiscall*)(CCameraSAInterface*, float radius, float x, float y, float z);
    ((ShakeCamera_t)FUNC_ShakeCam)(cameraInterface, radius, x, y, z);
}

void CCameraSA::ResetShakeCamera() noexcept
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;
    cameraInterface->m_fCamShakeForce = 0.0f;
}

std::uint8_t CCameraSA::GetTransitionState() const
{
    const CCameraSAInterface* cameraInterface = GetInterface();
    return cameraInterface ? cameraInterface->m_uiTransitionState : 0;
}

bool CCameraSA::IsInTransition() const
{
    return GetTransitionState() != 0;
}

float CCameraSA::GetTransitionFOV() const
{
    CCameraSAInterface* cameraInterface = GetInterface();
    return cameraInterface ? cameraInterface->FOVDuringInter : DEFAULT_FOV;
}

bool CCameraSA::GetTransitionMatrix(CMatrix& matrix) const
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface || !IsInTransition())
        return false;

    CVector source = cameraInterface->SourceDuringInter;
    CVector target = cameraInterface->TargetDuringInter;
    CVector up = cameraInterface->UpDuringInter;

    if (!IsFiniteVector(source) || !IsFiniteVector(target) || !IsFiniteVector(up))
        return false;

    CVector forward = target - source;
    if (forward.Length() < FLOAT_EPSILON)
        forward = CVector(0.0f, 1.0f, 0.0f);
    else
        forward.Normalize();

    CVector right = CVector(forward.fY, -forward.fX, 0.0f);
    if (right.Length() < FLOAT_EPSILON)
        right = CVector(1.0f, 0.0f, 0.0f);
    else
        right.Normalize();

    CVector correctedUp = right;
    correctedUp.CrossProduct(&forward);
    correctedUp.Normalize();

    matrix.vPos = source;
    matrix.vFront = forward;
    matrix.vRight = -right;
    matrix.vUp = correctedUp;
    matrix.OrthoNormalize(CMatrix::AXIS_FRONT, CMatrix::AXIS_UP);

    return true;
}

bool CCameraSA::IsSphereVisible(CVector* center, float radius) const
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return false;

    return ((bool(__thiscall*)(CCameraSAInterface*, CVector*, float))0x420D40)(cameraInterface, center, radius);
}

void CCameraSA::CopyCameraMatrixToRWCam(bool bUpdateMatrix) noexcept
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;

    using CopyCameraMatrixToRWCam_t = void(__thiscall*)(CCameraSAInterface*, bool);
    ((CopyCameraMatrixToRWCam_t)FUNC_CopyCameraMatrixToRWCam)(cameraInterface, bUpdateMatrix);
}

void CCameraSA::CalculateDerivedValues(bool bForMirror, bool bOriented) noexcept
{
    CCameraSAInterface* cameraInterface = GetInterface();
    if (!cameraInterface)
        return;

    using CalculateDerivedValues_t = void(__thiscall*)(CCameraSAInterface*, bool, bool);
    ((CalculateDerivedValues_t)FUNC_CalculateDerivedValues)(cameraInterface, bForMirror, bOriented);
}

bool CCameraSA::RenderWorldToRaster(CMatrix* cameraMatrix, RwRaster* targetRaster, RwRaster* targetZRaster) noexcept
{
    if (!cameraMatrix || !targetRaster)
        return false;

    RwCamera* pRwCamera = *reinterpret_cast<RwCamera**>(VAR_RwCameraPtr);
    if (!pRwCamera)
        return false;

    RwRaster* savedColorBuffer = pRwCamera->bufferColor;
    RwRaster* savedDepthBuffer = pRwCamera->bufferDepth;
    CMatrix   savedMatrix;
    GetMatrix(&savedMatrix);

    // Swap in the caller's target and camera, and push that camera into the live RW state,
    // exactly like CMirrors::BeforeMainRender does for the mirror's own offscreen pass.
    pRwCamera->bufferColor = targetRaster;
    pRwCamera->bufferDepth = targetZRaster;
    SetMatrix(cameraMatrix);
    CopyCameraMatrixToRWCam(true);
    CalculateDerivedValues(true, false);

    using RwCameraClear_t = void(__cdecl*)(RwCamera*, const void*, std::uint32_t);
    using RsCameraBeginUpdate_t = int(__cdecl*)(RwCamera*);
    using RwCameraEndUpdate_t = void(__cdecl*)(RwCamera*);
    using RenderSceneWorld_t = void(__cdecl*)();

    const std::uint32_t clearColour = 0xFF000000;
    // rwCAMERACLEARZ | rwCAMERACLEARIMAGE; the mirror path also ORs in rwCAMERACLEARSTENCIL for high
    // graphics quality, skipped here to avoid depending on that quality-check function too.
    constexpr std::uint32_t kClearFlags = 3;
    ((RwCameraClear_t)FUNC_RwCameraClear)(pRwCamera, &clearColour, kClearFlags);

    bool succeeded = false;
    if (((RsCameraBeginUpdate_t)FUNC_RsCameraBeginUpdate)(pRwCamera) != 0)
    {
        ((RenderSceneWorld_t)FUNC_RenderSceneWorld)();
        ((RwCameraEndUpdate_t)FUNC_RwCameraEndUpdate)(pRwCamera);
        succeeded = true;
    }

    // Restore the real camera and raster the same way CMirrors::RestoreCameraAfterMirror does.
    pRwCamera->bufferColor = savedColorBuffer;
    pRwCamera->bufferDepth = savedDepthBuffer;
    SetMatrix(&savedMatrix);
    CopyCameraMatrixToRWCam(true);
    CalculateDerivedValues(false, false);

    return succeeded;
}

int CCameraSA::GetScreenRasterDepth() const noexcept
{
    RwCamera* pRwCamera = *reinterpret_cast<RwCamera**>(VAR_RwCameraPtr);
    if (!pRwCamera || !pRwCamera->bufferColor)
        return 0;

    return pRwCamera->bufferColor->depth;
}

RwRaster* CCameraSA::CreateRaster(int width, int height, int depth, eRwRasterType type) noexcept
{
    using RwRasterCreate_t = RwRaster*(__cdecl*)(int, int, int, std::uint32_t);
    return ((RwRasterCreate_t)FUNC_RwRasterCreate)(width, height, depth, static_cast<std::uint32_t>(type));
}

void CCameraSA::DestroyRaster(RwRaster* raster) noexcept
{
    if (!raster)
        return;

    using RwRasterDestroy_t = int(__cdecl*)(RwRaster*);
    ((RwRasterDestroy_t)FUNC_RwRasterDestroy)(raster);
}

IDirect3DTexture9* CCameraSA::GetRasterTexture(RwRaster* raster) noexcept
{
    if (!raster)
        return nullptr;

    auto* pD3DRaster = reinterpret_cast<RwD3D9Raster*>(&raster->renderResource);
    return pD3DRaster->texture;
}

bool CCameraSA::DebugRenderWorldToFile(float offsetX, float offsetY, float offsetZ, const char* filePath) noexcept
{
    if (!filePath)
        return false;

    CMatrix currentMatrix;
    GetMatrix(&currentMatrix);

    CMatrix offsetMatrix = currentMatrix;
    offsetMatrix.vPos += CVector(offsetX, offsetY, offsetZ);

    const int depth = GetScreenRasterDepth();
    if (depth == 0)
        return false;

    constexpr int kTestRasterWidth = 512;
    constexpr int kTestRasterHeight = 256;

    RwRaster* colorRaster = CreateRaster(kTestRasterWidth, kTestRasterHeight, depth, eRwRasterType::CAMERATEXTURE);
    if (!colorRaster)
        return false;

    RwRaster* depthRaster = CreateRaster(kTestRasterWidth, kTestRasterHeight, depth, eRwRasterType::ZBUFFER);
    if (!depthRaster)
    {
        DestroyRaster(colorRaster);
        return false;
    }

    const bool renderSucceeded = RenderWorldToRaster(&offsetMatrix, colorRaster, depthRaster);
    bool       saveSucceeded = false;

    if (renderSucceeded)
    {
        IDirect3DTexture9* pTexture = GetRasterTexture(colorRaster);
        if (pTexture)
            saveSucceeded = SaveTextureAsBmp(pTexture, filePath);
    }

    DestroyRaster(depthRaster);
    DestroyRaster(colorRaster);

    return renderSucceeded && saveSucceeded;
}
