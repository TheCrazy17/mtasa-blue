/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CMultiplayerSA_FrameRateFixes.cpp
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

static bool         bWouldBeNewFrame = false;
static unsigned int nLastFrameTime = 0;

constexpr float kOriginalTimeStep = 50.0f / 30.0f;

// Fixes player movement issue while aiming and walking on high FPS.
#define HOOKPOS_CTaskSimpleUseGun__SetMoveAnim  0x61E4F2
#define HOOKSIZE_CTaskSimpleUseGun__SetMoveAnim 0x6
const unsigned int            RETURN_CTaskSimpleUseGun__SetMoveAnim = 0x61E4F8;
static void __declspec(naked) HOOK_CTaskSimpleUseGun__SetMoveAnim()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        fld ds:[0xB7CB5C]           // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fmul ds:[0x858B1C]          // 0.1f
        fxch
        fcom
        fxch
        fstp st(0)
        jmp RETURN_CTaskSimpleUseGun__SetMoveAnim
    }
    // clang-format on
}

// Fixes excessively fast camera shaking with setCameraShakeLevel on high FPS.
#define HOOKPOS_CCamera__Process  0x52C723
#define HOOKSIZE_CCamera__Process 0x12
static const unsigned int     RETURN_CCamera__Process = 0x52C735;
static void __declspec(naked) HOOK_CCamera__Process()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        fld ds:[0x858C80]           // 5.0f
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fadd ds:[0xB6EC30]
        fstp ds:[0xB6EC30]
        jmp RETURN_CCamera__Process
    }
    // clang-format on
}

// Fixes helicopters accelerating excessively during takeoff at high FPS.
#define HOOKPOS_CHeli__ProcessFlyingCarStuff  0x6C4F13
#define HOOKSIZE_CHeli__ProcessFlyingCarStuff 0x2A
static const unsigned int     RETURN_CHeli__ProcessFlyingCarStuff = 0x6C4F3D;
static void __declspec(naked) HOOK_CHeli__ProcessFlyingCarStuff()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        mov ax, [esi+0x22]
        cmp ax, 465
        jz is_rc_heli
        cmp ax, 501
        jz is_rc_heli

        fld ds:[0x858CDC]           // 0.001f
        jmp end

    is_rc_heli:
        fld ds:[0x859CD8]           // 0.003f

    end:
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fadd [esi+0x84C]
        jmp RETURN_CHeli__ProcessFlyingCarStuff
    }
    // clang-format on
}

// Fixes excessively fast movement of fog on high FPS.
#define HOOKPOS_CClouds__MovingFog_Update  0x716BA6
#define HOOKSIZE_CClouds__MovingFog_Update 0x16
static const unsigned int     RETURN_CClouds__MovingFog_Update = 0x716BBC;
static void __declspec(naked) HOOK_CClouds__MovingFog_Update()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        fmul [edi*4+0xC6E394]       // CClouds::ms_mf.fSpeedFactor
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fadd [esi]
        fstp [esi]
        fld [esp+0x18]
        fmul [edi*4+0xC6E394]       // CClouds::ms_mf.fSpeedFactor
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        jmp RETURN_CClouds__MovingFog_Update
    }
    // clang-format on
}

// Fixes glass shards spinning and moving at excessive speeds on high FPS.
#define HOOKPOS_CFallingGlassPane__Update_A  0x71AABF
#define HOOKSIZE_CFallingGlassPane__Update_A 0x6
static const unsigned int     RETURN_CFallingGlassPane__Update_A = 0x71AAC5;
static void __declspec(naked) HOOK_CFallingGlassPane__Update_A()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        fld [esp+0x28]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [esp+0x28]
        fld [esp+0x24]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [esp+0x24]
        fld [esp+0x20]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fadd [esi]
        jmp RETURN_CFallingGlassPane__Update_A
    }
    // clang-format on
}

// Fixes glass shards spinning and moving at excessive speeds on high FPS.
#define HOOKPOS_CFallingGlassPane__Update_B  0x71AAEA
#define HOOKSIZE_CFallingGlassPane__Update_B 0x6
static const unsigned int     RETURN_CFallingGlassPane__Update_B = 0x71AAF0;
static void __declspec(naked) HOOK_CFallingGlassPane__Update_B()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        fld [eax]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [eax]
        fld [eax+0x4]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [eax+0x4]
        fld [eax+0x8]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [eax+0x8]
        mov ecx, [eax]
        mov [esp+0x2C], ecx
        jmp RETURN_CFallingGlassPane__Update_B
    }
    // clang-format on
}

// Fixes glass shards spinning and moving at excessive speeds on high FPS.
#define HOOKPOS_CFallingGlassPane__Update_C  0x71AB29
#define HOOKSIZE_CFallingGlassPane__Update_C 0x6
static const unsigned int     RETURN_CFallingGlassPane__Update_C = 0x71AB2F;
static void __declspec(naked) HOOK_CFallingGlassPane__Update_C()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        fld [eax]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [eax]
        fld [eax+0x4]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [eax+0x4]
        fld [eax+0x8]
        fmul ds:[0xB7CB5C]          // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep      // 1.666f
        fstp [eax+0x8]
        mov edx, [eax]
        mov [esp+0x38], edx
        jmp RETURN_CFallingGlassPane__Update_C
    }
    // clang-format on
}

// Ensure that CTimer::CurrentFrame is updated only every 33+ milliseconds.
#define HOOKPOS_CTimer__Update  0x561C5D
#define HOOKSIZE_CTimer__Update 0xE
static void __declspec(naked) HOOK_CTimer__Update()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        add esp, 0x4

        mov bWouldBeNewFrame, 0
        mov eax, nLastFrameTime
        add eax, 33                 // 33 = 1000 / 30
        mov ecx, ds:[0xB7CB84]      // CTimer::m_snTimeInMilliseconds
        cmp ecx, eax
        jb skip

        mov bWouldBeNewFrame, 1
        mov nLastFrameTime, ecx
        mov eax, ds:[0xB7CB4C]      // CTimer::m_FrameCounter
        inc eax
        mov ds:[0xB7CB4C], eax      // CTimer::m_FrameCounter

    skip:
        add esp, 0xC
        ret
    }
    // clang-format on
}

// Fixes premature despawning of broken breakable objects on high FPS.
#define HOOKPOS_BreakObject_c__Update  0x59E420
#define HOOKSIZE_BreakObject_c__Update 0xB
static const unsigned int     RETURN_BreakObject_c__Update = 0x59E42B;
static void __declspec(naked) HOOK_BreakObject_c__Update()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        mov edx, [edi+eax+0x70]
        lea eax, [edi+eax+0x70]
        dec edx
        mov [eax], edx

    skip:
        jmp RETURN_BreakObject_c__Update
    }
    // clang-format on
}

// Fixes limited reach of the water cannon on high FPS.
#define HOOKPOS_CWaterCannon__Update_OncePerFrame  0x72A29B
#define HOOKSIZE_CWaterCannon__Update_OncePerFrame 0x5
static const unsigned int     RETURN_CWaterCannon__Update_OncePerFrame = 0x72A2A0;
static const unsigned int     RETURN_CWaterCannon__Update_OncePerFrame_SKIP = 0x72A2BB;
static void __declspec(naked) HOOK_CWaterCannon__Update_OncePerFrame()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        movsx eax, [edi+0x4]
        inc eax

        jmp RETURN_CWaterCannon__Update_OncePerFrame
    skip:
        jmp RETURN_CWaterCannon__Update_OncePerFrame_SKIP
    }
    // clang-format on
}

// Fixes money animation issues on high FPS.
#define HOOKPOS_CPlayerInfo__Process  0x5700F5
#define HOOKSIZE_CPlayerInfo__Process 0x6
static const unsigned int     RETURN_CPlayerInfo__Process = 0x5700FB;
static const unsigned int     RETURN_CPlayerInfo__Process_SKIP = 0x57015B;
static void __declspec(naked) HOOK_CPlayerInfo__Process()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        mov edx, [esi+0xBC]

        jmp RETURN_CPlayerInfo__Process
    skip:
        jmp RETURN_CPlayerInfo__Process_SKIP
    }
    // clang-format on
}

// Fixes excessive effects spawning from rocket launchers on high FPS.
#define HOOKPOS_CProjectileInfo__Update  0x738C63
#define HOOKSIZE_CProjectileInfo__Update 0x5
static const unsigned int     RETURN_CProjectileInfo__Update = 0x738C68;
static const unsigned int     RETURN_CProjectileInfo__Update_SKIP = 0x738F22;
static void __declspec(naked) HOOK_CProjectileInfo__Update()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        mov eax, [ebx]
        cmp eax, 0x13

        jmp RETURN_CProjectileInfo__Update
    skip:
        jmp RETURN_CProjectileInfo__Update_SKIP
    }
    // clang-format on
}

// Fixes excessive surface effects spawning from wheels on high FPS.
#define HOOKPOS_CVehicle__AddWheelDirtAndWater  0x6D2D50
#define HOOKSIZE_CVehicle__AddWheelDirtAndWater 0x6
static const unsigned int     RETURN_CVehicle__AddWheelDirtAndWater = 0x6D2D56;
static void __declspec(naked) HOOK_CVehicle__AddWheelDirtAndWater()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        mov eax, [esp+0x8]
        test eax, eax

        jmp RETURN_CVehicle__AddWheelDirtAndWater
    skip:
        xor eax, eax
        retn 0x10
    }
    // clang-format on
}

// Fixes excessive smoke trail particle spawning from stuntplanes and cropdusters on high FPS.
#define HOOKPOS_CPlane__PreRender  0x6CA937
#define HOOKSIZE_CPlane__PreRender 0x6
static const unsigned int     RETURN_CPlane__PreRender = 0x6CA93D;
static const unsigned int     RETURN_CPlane__PreRender_SKIP = 0x6CAA93;
static void __declspec(naked) HOOK_CPlane__PreRender()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        mov al, [esi+0xA00]

        jmp RETURN_CPlane__PreRender
    skip:
        jmp RETURN_CPlane__PreRender_SKIP
    }
    // clang-format on
}

// Fixes increased frequency of water cannon pushing peds on high FPS.
#define HOOKPOS_CWaterCannon__Update_OncePerFrame_PushPedFix  0x72A37B
#define HOOKSIZE_CWaterCannon__Update_OncePerFrame_PushPedFix 0x6
static const unsigned int     RETURN_CWaterCannon__Update_OncePerFrame_PushPedFix = 0x72A381;
static const unsigned int     RETURN_CWaterCannon__Update_OncePerFrame_PushPedFix_SKIP = 0x72A38E;
static void __declspec(naked) HOOK_CWaterCannon__Update_OncePerFrame_PushPedFix()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        mov ecx, ds:[0xB7CB4C]

        jmp RETURN_CWaterCannon__Update_OncePerFrame_PushPedFix
    skip:
        jmp RETURN_CWaterCannon__Update_OncePerFrame_PushPedFix_SKIP
    }
    // clang-format on
}

// Fixes excessive particle spawning from water cannons on high FPS.
#define HOOKPOS_CWaterCannon__Render_FxFix  0x729437
#define HOOKSIZE_CWaterCannon__Render_FxFix 0x5
static const unsigned int     RETURN_CWaterCannon__Render_FxFix = 0x729440;
static const unsigned int     RETURN_CWaterCannon__Render_FxFix_SKIP = 0x7294EE;
static void __declspec(naked) HOOK_CWaterCannon__Render_FxFix()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        jmp RETURN_CWaterCannon__Render_FxFix
    skip:
        jmp RETURN_CWaterCannon__Render_FxFix_SKIP
    }
    // clang-format on
}

// Fixes excessive particle spawning with setPedHeadless on high FPS.
#define HOOKPOS_CPed__PreRenderAfterTest  0x5E7181
#define HOOKSIZE_CPed__PreRenderAfterTest 0x6
static const unsigned int     RETURN_CPed__PreRenderAfterTest = 0x5E7187;
static const unsigned int     RETURN_CPed__PreRenderAfterTest_SKIP = 0x5E722D;
static void __declspec(naked) HOOK_CPed__PreRenderAfterTest()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        mov eax, [ebp+0x46C]

        jmp RETURN_CPed__PreRenderAfterTest
    skip:
        jmp RETURN_CPed__PreRenderAfterTest_SKIP
    }
    // clang-format on
}

// Fixes excessive particle spawning from boats on high FPS.
#define HOOKPOS_cBuoyancy__AddSplashParticles  0x6C34E0
#define HOOKSIZE_cBuoyancy__AddSplashParticles 0x6
static const unsigned int     RETURN_cBuoyancy__AddSplashParticles = 0x6C34E6;
static void __declspec(naked) HOOK_cBuoyancy__AddSplashParticles()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        sub esp, 0xE0

        jmp RETURN_cBuoyancy__AddSplashParticles
    skip:
        retn 0x2C
    }
    // clang-format on
}

// Fixes excessive weather particle spawning on high FPS.
#define HOOKPOS_CWeather__AddRain  0x72AAA8
#define HOOKSIZE_CWeather__AddRain 0x6
static const unsigned int     RETURN_CWeather__AddRain = 0x72AAAE;
static void __declspec(naked) HOOK_CWeather__AddRain()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        fld ds:[0xC812F0]

        jmp RETURN_CWeather__AddRain

    skip:
        add esp, 0x84
        ret
    }
    // clang-format on
}

// Fixes excessive damage particle spawning from airplanes on high FPS.
#define HOOKPOS_CPlane__ProcessFlyingCarStuff  0x6CBE4B
#define HOOKSIZE_CPlane__ProcessFlyingCarStuff 0x6
static const unsigned int     RETURN_CPlane__ProcessFlyingCarStuff = 0x6CBE51;
static const unsigned int     RETURN_CPlane__ProcessFlyingCarStuff_SKIP = 0x6CC0D9;
static void __declspec(naked) HOOK_CPlane__ProcessFlyingCarStuff()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        mov edx, ds:[0xB6F03C]

        jmp RETURN_CPlane__ProcessFlyingCarStuff
    skip:
        jmp RETURN_CPlane__ProcessFlyingCarStuff_SKIP
    }
    // clang-format on
}

// Fixes excessive spawning of sand and water particles from vehicles on high FPS.
#define HOOKPOS_CAutomobile__UpdateWheelMatrix  0x6AA78A
#define HOOKSIZE_CAutomobile__UpdateWheelMatrix 0x5
static const unsigned int     RETURN_CAutomobile__UpdateWheelMatrix = 0x6AA78F;
static const unsigned int     RETURN_CAutomobile__UpdateWheelMatrix_SKIP = 0x6AAAD0;
static void __declspec(naked) HOOK_CAutomobile__UpdateWheelMatrix()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        push 0x3D4CCCCD

        jmp RETURN_CAutomobile__UpdateWheelMatrix
    skip:
        jmp RETURN_CAutomobile__UpdateWheelMatrix_SKIP
    }
    // clang-format on
}

// Fixes excessive particle spawning from boats on high FPS.
#define HOOKPOS_CVehicle__DoBoatSplashes  0x6DD130
#define HOOKSIZE_CVehicle__DoBoatSplashes 0x6
static const unsigned int     RETURN_CVehicle__DoBoatSplashes = 0x6DD136;
static void __declspec(naked) HOOK_CVehicle__DoBoatSplashes()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        sub esp, 0x80

        jmp RETURN_CVehicle__DoBoatSplashes
    skip:
        retn 4
    }
    // clang-format on
}

// Fixes excessive rain particle spawning on vehicles on high FPS.
#define HOOKPOS_CVehicle__AddWaterSplashParticles  0x6DDF60
#define HOOKSIZE_CVehicle__AddWaterSplashParticles 0x6
static const unsigned int     RETURN_CVehicle__AddWaterSplashParticles = 0x6DDF66;
static void __declspec(naked) HOOK_CVehicle__AddWaterSplashParticles()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        sub esp, 0xC4

        jmp RETURN_CVehicle__AddWaterSplashParticles
    skip:
        ret
    }
    // clang-format on
}

// Fixes excessive particle spawning from airplanes when damaged on high FPS.
#define HOOKPOS_CPlane__ProcessControl  0x6C939A
#define HOOKSIZE_CPlane__ProcessControl 0x5
static const unsigned int     RETURN_CPlane__ProcessControl = 0x6C939F;
static const unsigned int     RETURN_CPlane__ProcessControl_SKIP = 0x6C9463;
static void __declspec(naked) HOOK_CPlane__ProcessControl()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        lea ecx, [esp+0x3C]
        push ecx

        jmp RETURN_CPlane__ProcessControl
    skip:
        jmp RETURN_CPlane__ProcessControl_SKIP
    }
    // clang-format on
}

// Fixes excessive exhaust particle spawning from vehicles on high FPS.
#define HOOKPOS_CVehicle__AddExhaustParticles  0x6DE240
#define HOOKSIZE_CVehicle__AddExhaustParticles 0x6
static const unsigned int     RETURN_CVehicle__AddExhaustParticles = 0x6DE246;
static void __declspec(naked) HOOK_CVehicle__AddExhaustParticles()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx edx, bWouldBeNewFrame
        test edx, edx
        jz skip

        mov eax, fs:0x0

        jmp RETURN_CVehicle__AddExhaustParticles
    skip:
        ret
    }
    // clang-format on
}

// Fixes excessive particle spawning while swimming on high FPS.
#define HOOKPOS_CTaskSimpleSwim__ProcessEffects  0x68AD3B
#define HOOKSIZE_CTaskSimpleSwim__ProcessEffects 0x6
static const unsigned int     RETURN_CTaskSimpleSwim__ProcessEffects = 0x68AD41;
static const unsigned int     RETURN_CTaskSimpleSwim__ProcessEffects_SKIP = 0x68AFDB;
static void __declspec(naked) HOOK_CTaskSimpleSwim__ProcessEffects()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        mov eax, [edi+0x14]
        add eax, 0x10

        jmp RETURN_CTaskSimpleSwim__ProcessEffects
    skip:
        jmp RETURN_CTaskSimpleSwim__ProcessEffects_SKIP
    }
    // clang-format on
}

// Fixes excessive particle spawning while swimming on high FPS.
#define HOOKPOS_CTaskSimpleSwim__ProcessEffectsBubbleFix  0x68AC31
#define HOOKSIZE_CTaskSimpleSwim__ProcessEffectsBubbleFix 0x7
static const unsigned int     RETURN_CTaskSimpleSwim__ProcessEffectsBubbleFix = 0x68AC38;
static const unsigned int     RETURN_CTaskSimpleSwim__ProcessEffectsBubbleFix_SKIP = 0x68AD36;
static void __declspec(naked) HOOK_CTaskSimpleSwim__ProcessEffectsBubbleFix()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        movzx eax, bWouldBeNewFrame
        test eax, eax
        jz skip

        mov ecx, edi
        mov esi, 5

        jmp RETURN_CTaskSimpleSwim__ProcessEffectsBubbleFix
    skip:
        jmp RETURN_CTaskSimpleSwim__ProcessEffectsBubbleFix_SKIP
    }
    // clang-format on
}

// Fixes invisible weapon particles (extinguisher, spraycan, flamethrower) at high FPS
#define HOOKPOS_CWeapon_Update  0x73DC3D
#define HOOKSIZE_CWeapon_Update 5
static constexpr std::uintptr_t RETURN_CWeapon_Update = 0x073DC42;
static void __declspec(naked)   HOOK_CWeapon_Update()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        // Temp fix for camera
        cmp [esi], 0x2B // CWeapon::m_eWeaponType
        je skip

        // timeStep / kOriginalTimeStep
        fld ds:[0xB7CB5C] // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep

        mov eax, [esi+10h] // m_timeToNextShootInMS
        mov ebx, ds:[0xB7CB84] // CTimer::m_snTimeInMilliseconds

        sub eax, ebx // m_timeToNextShootInMS - CTimer::m_snTimeInMilliseconds

        push eax
        fild dword ptr [esp]
        add esp, 4

        fmul st(0), st(1) // (m_timeToNextShootInMS - CTimer::m_snTimeInMilliseconds) * (timeStep / kOriginalTimeStep)
        fadd st(0), ebx // + m_snTimeInMilliseconds
        fistp [esi+10h]
        fstp st(0)

        mov eax, ebx

        xor ebx, ebx
        jmp RETURN_CWeapon_Update

        skip:
        mov eax, ds:[0xB7CB84]
        jmp RETURN_CWeapon_Update
    }
    // clang-format on
}

#define HOOKPOS_CPhysical__ApplyAirResistance  0x544D29
#define HOOKSIZE_CPhysical__ApplyAirResistance 5
static const unsigned int     RETURN_CPhysical__ApplyAirResistance = 0x544D4D;
static void __declspec(naked) HOOK_CPhysical__ApplyAirResistance()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        fld ds:[0x862CD0]            // 0.99000001f
        fld ds:[0xB7CB5C]            // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep            // 1.666f
        mov eax, 0x822130            // powf
        call eax

        fld st(0)
        fmul [esi+0x50]
        fstp [esi+0x50]

        fld st(0)
        fmul [esi+0x54]
        fstp [esi+0x54]

        fmul [esi+0x58]
        fstp [esi+0x58]
        jmp RETURN_CPhysical__ApplyAirResistance
    }
    // clang-format on
}

template <unsigned int returnAddress>
static void __declspec(naked) HOOK_VehicleRapidStopFix()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    static unsigned int RETURN_VehicleRapidStopFix = returnAddress;
    // clang-format off
    __asm
    {
        fld ds:[0xC2B9CC]            // mod_HandlingManager.m_fWheelFriction
        fmul ds:[0xB7CB5C]            // CTimer::ms_fTimeStep
        fdiv kOriginalTimeStep            // 1.666f
        jmp RETURN_VehicleRapidStopFix
    }
    // clang-format on
}

void CMultiplayerSA::SetRapidVehicleStopFixEnabled(bool enabled)
{
    if (m_isRapidVehicleStopFixEnabled == enabled)
        return;

    if (enabled)
    {
        EZHookInstall(CPhysical__ApplyAirResistance);

        // CVehicle::ProcessWheel
        HookInstall(0x6D6E69, (DWORD)HOOK_VehicleRapidStopFix<0x6D6E6F>, 6);
        HookInstall(0x6D6EA8, (DWORD)HOOK_VehicleRapidStopFix<0x6D6EAE>, 6);

        // CVehicle::ProcessBikeWheel
        HookInstall(0x6D767F, (DWORD)HOOK_VehicleRapidStopFix<0x6D7685>, 6);
        HookInstall(0x6D76AB, (DWORD)HOOK_VehicleRapidStopFix<0x6D76B1>, 6);
        HookInstall(0x6D76CD, (DWORD)HOOK_VehicleRapidStopFix<0x6D76D3>, 6);
    }
    else
    {
        MemCpy((void*)HOOKPOS_CPhysical__ApplyAirResistance, "\xD9\x46\x50\xD8\x0D", 5);

        MemCpy((void*)0x6D6E69, "\xD9\x05\xCC\xB9\xC2\x00", 6);
        MemCpy((void*)0x6D6EA8, "\xD9\x05\xCC\xB9\xC2\x00", 6);

        MemCpy((void*)0x6D767F, "\xD9\x05\xCC\xB9\xC2\x00", 6);
        MemCpy((void*)0x6D76AB, "\xD9\x05\xCC\xB9\xC2\x00", 6);
        MemCpy((void*)0x6D76CD, "\xD9\x05\xCC\xB9\xC2\x00", 6);
    }

    m_isRapidVehicleStopFixEnabled = enabled;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CBike::ProcessControl - stationary lean-target noise suppression
//
// Fix for #4658: bicycles wobble side to side at very high FPS while stationary with a
// rider, but only bicycles, only standing still, and only with someone on board.
//
// The rider-balanced lean target divides this frame's lateral velocity delta ("raw", on
// ST(1) at this point) by max(CTimer::ms_fTimeStep, epsilon) * a small constant, then
// asin()'s and clamps the result. While actually moving, that delta is real acceleration
// times the frame time, so dividing by the frame time cancels out and the result is
// frame-rate independent, same as every other pow(k, ms_fTimeStep) blend in this function.
// While stationary, the delta is instead contact-solver impulse noise from the wheels,
// which does not shrink proportionally with the timestep - dividing constant-sized noise
// by an ever-smaller frame time amplifies it by roughly 1/dt. At high FPS that amplified,
// sign-alternating value saturates the lean clamp every frame and is copied straight into
// CBike::CalculateLeanMatrix's LeanAngle, which is what actually renders the wobble.
// Bicycles show it and motorcycles don't because bicycles' much lower handling mass makes
// the same solver noise produce a proportionally larger velocity delta; there is no
// bicycle-specific branch anywhere in this code path.
//
// An earlier version of this fix floored the divisor instead of zeroing the numerator.
// That only reduces the gain on the noise - if the noise was already big enough to
// saturate the +-fMaxLean clamp before the floor, dividing it by a bigger number still
// leaves it clamped to the same value, so nothing changes. Zeroing the velocity delta
// itself removes the noise at the source instead of just turning its gain down, so the
// lean target relaxes toward the same rest pose the un-ridden/parked bike already decays
// to elsewhere in this function, through the same frame-rate-independent pow(fDesLean, dt)
// blend - no snapping, no clamp to fight.
//
// Gated on the same stationary velocity threshold the game itself already uses a few
// instructions earlier in this function, so moving, motorcycles, wheelies and AI riders
// are unaffected. Unlike that gate, the steer check here uses the same small epsilon as
// the velocity checks instead of exact-zero equality: m_fSteerAngle relaxes to 0 through
// its own per-frame decay (see the BarSteerAngle branch above) and can sit at a tiny but
// not bit-exact-zero value for a noticeable stretch of real time while doing so - an
// exact-equality gate leaves that whole stretch fully unmitigated, which traced logging
// confirmed is exactly where the wobble was still showing up right after braking to a
// stop, before the steer angle finished decaying to bit-exact zero.
//
//////////////////////////////////////////////////////////////////////////////////////////
// >>> 0x6BBAF7 | D8 0D 84 39 86 00 | fmul dword ptr ds:[00863984h]  ; * small scale constant
#define HOOKPOS_CBike__ProcessControl_StationaryLeanNoiseSuppress  0x6BBAF7
#define HOOKSIZE_CBike__ProcessControl_StationaryLeanNoiseSuppress 6
static const unsigned int RETURN_CBike__ProcessControl_StationaryLeanNoiseSuppress = 0x6BBAFD;

static const float fStationaryLeanVelocityThreshold = 0.01f;

// TEMP TRACE for #4658 - remove before commit
static void*        g_bike4658Esi;
static float         g_bike4658VelX;
static float         g_bike4658VelY;
static float         g_bike4658SteerAngle;
static float         g_bike4658Raw;
static float         g_bike4658Divisor;
static int            g_bike4658Stationary;

// TEMP TRACE for #4658 - remove before commit
static void LogBike4658Frame()
{
    if (FILE* f = fopen("C:\\bike4658_trace.log", "a"))
    {
        fprintf(f, "[%lu] bike=%p velX=%.6f velY=%.6f steer=%.6f dt=%.6f raw=%.6f divisor=%.6f stationary=%d\n", GetTickCount32(), g_bike4658Esi,
                g_bike4658VelX, g_bike4658VelY, g_bike4658SteerAngle, *(float*)0x00B7CB5C, g_bike4658Raw, g_bike4658Divisor, g_bike4658Stationary);
        fclose(f);
    }
}

static void __declspec(naked) HOOK_CBike__ProcessControl_StationaryLeanNoiseSuppress()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        // esi = CBike*. ST(0) = max(ms_fTimeStep, epsilon) as computed by the unmodified
        // code above; ST(1) = this frame's raw lateral velocity delta. Flush both to
        // memory immediately so the FPU stack is empty (needed for the temporary trace
        // call below - remove this flush/reload dance along with the trace when done).
        fstp    dword ptr [g_bike4658Divisor]
        fstp    dword ptr [g_bike4658Raw]

        mov     g_bike4658Esi, esi
        mov     eax, dword ptr [esi+0x44]
        mov     g_bike4658VelX, eax
        mov     eax, dword ptr [esi+0x48]
        mov     g_bike4658VelY, eax
        mov     eax, dword ptr [esi+0x494]
        mov     g_bike4658SteerAngle, eax
        mov     g_bike4658Stationary, 0

        // Every check below is FPU-stack neutral (one push, one popping compare).
        fld     dword ptr [esi+0x44]           // m_vecMoveSpeed.x
        fabs
        fcomp   fStationaryLeanVelocityThreshold
        fnstsw  ax
        sahf
        jae     notStationary                  // |velX| >= threshold -> not stationary

        fld     dword ptr [esi+0x48]           // m_vecMoveSpeed.y
        fabs
        fcomp   fStationaryLeanVelocityThreshold
        fnstsw  ax
        sahf
        jae     notStationary                  // |velY| >= threshold -> not stationary

        fld     dword ptr [esi+0x494]          // m_fSteerAngle
        fabs
        fcomp   fStationaryLeanVelocityThreshold
        fnstsw  ax
        sahf
        jae     notStationary                  // |steerAngle| >= threshold -> not stationary

        mov     g_bike4658Stationary, 1

        notStationary:
        pushad
        call    LogBike4658Frame
        popad

        // Reconstruct ST(1) (raw: 0 if stationary, else the original) and ST(0) (the
        // divisor, unchanged) - the division result will be 0 regardless of the divisor's
        // value when stationary, since the divisor is never 0 (it is itself an epsilon-
        // floored max()).
        cmp     g_bike4658Stationary, 0
        jz      useOriginalRaw
        fldz
        jmp     pushDivisor
        useOriginalRaw:
        fld     dword ptr [g_bike4658Raw]
        pushDivisor:
        fld     dword ptr [g_bike4658Divisor]

        fmul    dword ptr ds:[00863984h]
        jmp     RETURN_CBike__ProcessControl_StationaryLeanNoiseSuppress
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CBmx::ProcessControl - stationary pedal-lean suppression
//
// Second part of the #4658 fix: bicycles (CBmx only - BMX/mountain bike/etc, not
// motorcycles) run this on top of CBike::ProcessControl every frame. Whenever the pedal
// animation still has more than 1% blend weight (i.e. for a little while after the rider
// stops pedaling, while that animation is fading out), it unconditionally does
// LeanAngle += sin(pedalPhase) * blendAmount * a small constant - with no check at all for
// whether the bike is actually moving. CBike::ProcessControl (patched above) runs first
// each frame and, while stationary, now settles LeanAngle back to ~0; this addition then
// perturbs it again on top of that, every single frame, for as long as the pedal
// animation's blend weight takes to fade out - which is why a little wobble remained at
// specific points in the pedal cycle even after the main fix, most noticeably right after
// stopping.
//
// Suppressed under the same stationary condition as the main fix, so the pedalling lean
// while actually riding is untouched.
//
//////////////////////////////////////////////////////////////////////////////////////////
// >>> 0x6BFB29 | D8 86 48 06 00 00 | fadd dword ptr [esi+648h]  ; LeanAngle += pedal lean
#define HOOKPOS_CBmx__ProcessControl_StationaryPedalLeanSuppress  0x6BFB29
#define HOOKSIZE_CBmx__ProcessControl_StationaryPedalLeanSuppress 6
static const unsigned int RETURN_CBmx__ProcessControl_StationaryPedalLeanSuppress = 0x6BFB2F;

// TEMP TRACE for #4658 - remove before commit
static void*  g_bmx4658Esi;
static float  g_bmx4658Addend;
static float  g_bmx4658LeanAngleBefore;
static int    g_bmx4658Stationary;

// TEMP TRACE for #4658 - remove before commit
static void LogBmx4658Frame()
{
    if (FILE* f = fopen("C:\\bike4658_trace.log", "a"))
    {
        fprintf(f, "[%lu] BMX bike=%p addend=%.6f leanBefore=%.6f stationary=%d\n", GetTickCount32(), g_bmx4658Esi, g_bmx4658Addend,
                g_bmx4658LeanAngleBefore, g_bmx4658Stationary);
        fclose(f);
    }
}

static void __declspec(naked) HOOK_CBmx__ProcessControl_StationaryPedalLeanSuppress()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        // esi = CBmx* (same base CBike layout). ST(0) = the pedal-lean addend about to be
        // added to LeanAngle at [esi+0x648]. Flush it to memory so the FPU stack is empty
        // for the temporary trace call below.
        fstp    dword ptr [g_bmx4658Addend]

        mov     g_bmx4658Esi, esi
        mov     eax, dword ptr [esi+0x648]
        mov     g_bmx4658LeanAngleBefore, eax
        mov     g_bmx4658Stationary, 0

        fld     dword ptr [esi+0x44]           // m_vecMoveSpeed.x
        fabs
        fcomp   fStationaryLeanVelocityThreshold
        fnstsw  ax
        sahf
        jae     bmxNotStationary

        fld     dword ptr [esi+0x48]           // m_vecMoveSpeed.y
        fabs
        fcomp   fStationaryLeanVelocityThreshold
        fnstsw  ax
        sahf
        jae     bmxNotStationary

        fld     dword ptr [esi+0x494]          // m_fSteerAngle
        fabs
        fcomp   fStationaryLeanVelocityThreshold
        fnstsw  ax
        sahf
        jae     bmxNotStationary

        mov     g_bmx4658Stationary, 1
        fldz
        fstp    dword ptr [g_bmx4658Addend]    // stationary: suppress the addend

        bmxNotStationary:
        pushad
        call    LogBmx4658Frame
        popad

        fld     dword ptr [g_bmx4658Addend]
        fadd    dword ptr [esi+0x648]
        jmp     RETURN_CBmx__ProcessControl_StationaryPedalLeanSuppress
    }
    // clang-format on
}

void CMultiplayerSA::InitHooks_FrameRateFixes()
{
    EZHookInstall(CBike__ProcessControl_StationaryLeanNoiseSuppress);
    EZHookInstall(CBmx__ProcessControl_StationaryPedalLeanSuppress);

    EZHookInstall(CTaskSimpleUseGun__SetMoveAnim);
    EZHookInstall(CCamera__Process);
    EZHookInstall(CHeli__ProcessFlyingCarStuff);
    EZHookInstall(CClouds__MovingFog_Update);
    EZHookInstall(CFallingGlassPane__Update_A);
    EZHookInstall(CFallingGlassPane__Update_B);
    EZHookInstall(CFallingGlassPane__Update_C);

    // Fixes slow camera movement towards the back of the vehicle on high FPS.
    // CCam::Process_FollowCar_SA
    MemSet((void*)0x524FD7, 0x90, 0x1B);

    // Fixes slow boat movement on high FPS.
    // CVehicle::ProcessBoatControl
    MemPut(0x6DC23F, &kOriginalTimeStep);

    // Fixes climbing over certain objects killing player on high FPS or low game speed.
    // GitHub Issue #602
    MemPut(0x6811E9, &kOriginalTimeStep);
    MemPut(0x68128A, &kOriginalTimeStep);
    MemPut(0x68131B, &kOriginalTimeStep);

    // CTimer::m_FrameCounter fixes
    EZHookInstall(CTimer__Update);

    EZHookInstall(BreakObject_c__Update);
    EZHookInstall(CWaterCannon__Update_OncePerFrame);
    EZHookInstall(CPlayerInfo__Process);

    EZHookInstall(CProjectileInfo__Update);
    EZHookInstall(CVehicle__AddWheelDirtAndWater);
    EZHookInstall(CPlane__PreRender);
    EZHookInstall(CWaterCannon__Update_OncePerFrame_PushPedFix);
    EZHookInstall(CWaterCannon__Render_FxFix);
    EZHookInstall(CPed__PreRenderAfterTest);
    EZHookInstall(cBuoyancy__AddSplashParticles);
    EZHookInstall(CWeather__AddRain);
    EZHookInstall(CPlane__ProcessFlyingCarStuff);
    EZHookInstall(CAutomobile__UpdateWheelMatrix);
    EZHookInstall(CVehicle__DoBoatSplashes);
    EZHookInstall(CVehicle__AddWaterSplashParticles);
    EZHookInstall(CPlane__ProcessControl);
    EZHookInstall(CVehicle__AddExhaustParticles);
    EZHookInstall(CTaskSimpleSwim__ProcessEffects);
    EZHookInstall(CTaskSimpleSwim__ProcessEffectsBubbleFix);

    EZHookInstall(CWeapon_Update);
}
