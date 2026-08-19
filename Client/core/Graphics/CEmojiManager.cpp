/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/Graphics/CEmojiManager.cpp
 *  PURPOSE:     Emoji texture lookup class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CEmojiManager.h"

namespace
{
    struct SEmojiTableEntry
    {
        const char* szShortcode;            // Name without colons, e.g. "heart"
        char32_t    codepoint;              // Primary Unicode code point, or 0 if it has none
        const char* szFile;                 // PNG filename in MTA/cgui/images/emoji
    };

    // clang-format off
    const SEmojiTableEntry g_EmojiTable[] = {
        {"grinning",                 0x1F600, "grinning_face.png"},
        {"joy",                      0x1F602, "face_with_tears_of_joy.png"},
        {"rofl",                     0x1F923, "rolling_on_the_floor_laughing.png"},
        {"cry",                      0x1F622, "crying_face.png"},
        {"open_mouth",               0x1F62E, "face_with_open_mouth.png"},
        {"rage",                     0x1F621, "pouting_face.png"},
        {"sunglasses",               0x1F60E, "smiling_face_with_sunglasses.png"},
        {"thumbsup",                 0x1F44D, "thumbs_up.png"},
        {"thumbsdown",               0x1F44E, "thumbs_down.png"},
        {"heart",                    0x2764,  "red_heart.png"},
        {"broken_heart",             0x1F494, "broken_heart.png"},
        {"fire",                     0x1F525, "fire.png"},
        {"skull",                    0x1F480, "skull.png"},
        {"tada",                     0x1F389, "party_popper.png"},
        {"clap",                     0x1F44F, "clapping_hands.png"},
        {"pray",                     0x1F64F, "folded_hands.png"},
        {"white_check_mark",         0x2705,  "check_mark_button.png"},
        {"x",                        0x274C,  "cross_mark.png"},
        {"slightly_smiling_face",    0x1F642, "slightly_smiling_face.png"},
        {"slightly_frowning_face",   0x1F641, "slightly_frowning_face.png"},
        {"wink",                     0x1F609, "winking_face.png"},
        {"stuck_out_tongue",         0x1F61B, "face_with_tongue.png"},
        {"new_moon_face",            0x1F31A, "new_moon_face.png"},
        {"warning",                  0x26A0,  "warning.png"},
        {"smile",                    0x1F601, "beaming_face_with_smiling_eyes.png"},
        {"blush",                    0x1F60A, "smiling_face_with_smiling_eyes.png"},
        {"heart_eyes",               0x1F60D, "smiling_face_with_heart-eyes.png"},
        {"kissing_heart",            0x1F618, "face_blowing_a_kiss.png"},
        {"sob",                      0x1F62D, "loudly_crying_face.png"},
        {"scream",                   0x1F631, "face_screaming_in_fear.png"},
        {"thinking",                 0x1F914, "thinking_face.png"},
        {"roll_eyes",                0x1F644, "face_with_rolling_eyes.png"},
        {"eyes",                     0x1F440, "eyes.png"},
        {"100",                      0x1F4AF, "hundred_points.png"},
        {"handshake",                0x1F91D, "handshake.png"},
        {"wave",                     0x1F44B, "waving_hand.png"},
        {"muscle",                   0x1F4AA, "flexed_biceps.png"},
        {"facepalm",                 0x1F926, "person_facepalming.png"},
        {"partying_face",            0x1F973, "partying_face.png"},
        {"poop",                     0x1F4A9, "pile_of_poo.png"},
        {"crown",                    0x1F451, "crown.png"},
        {"video_game",               0x1F3AE, "video_game.png"},
    };
    // clang-format on
}  // namespace

void CEmojiManager::LoadTextures(CRenderItemManagerInterface* pRenderItemManager)
{
    for (const SEmojiTableEntry& entry : g_EmojiTable)
    {
        SString strPath = CalcMTASAPath(SString("MTA\\cgui\\images\\emoji\\%s", entry.szFile));

        CTextureItem* pTexture = pRenderItemManager->CreateTexture(strPath, nullptr, false, -1, -1, RFORMAT_DXT3, TADDRESS_CLAMP);
        if (!pTexture)
            continue;

        m_ShortcodeMap[entry.szShortcode] = pTexture;
        if (entry.codepoint != 0)
            m_CodepointMap[entry.codepoint] = pTexture;
    }
}

void CEmojiManager::ReleaseTextures()
{
    // Every texture is referenced once from m_ShortcodeMap, codepoint entries just point at the same instance
    for (auto& pair : m_ShortcodeMap)
        SAFE_RELEASE(pair.second);

    m_ShortcodeMap.clear();
    m_CodepointMap.clear();
}

bool CEmojiManager::ContainsTrigger(const std::wstring& wstrText) const
{
    // Reuses TryMatch itself so a string is only ever flagged when it actually resolves to
    // an emoji, e.g. plain text with a stray ':' (like "12:30") must not lose its fast draw path.
    const wchar_t* pText = wstrText.c_str();
    while (*pText != L'\0')
    {
        uint uiLength = 0;
        if (TryMatch(pText, uiLength))
            return true;
        pText++;
    }
    return false;
}

bool CEmojiManager::TryMatchAtEnd(const std::wstring& wstrText, uint& outLength) const
{
    // The longest token is a :shortcode: — cap the search window to the longest table entry plus a margin
    const size_t   uiMaxTokenLength = 34;
    const size_t   size = wstrText.size();
    const size_t   searchStart = size > uiMaxTokenLength ? size - uiMaxTokenLength : 0;
    const wchar_t* pData = wstrText.c_str();

    for (size_t i = searchStart; i < size; i++)
    {
        uint uiLength = 0;
        if (TryMatch(&pData[i], uiLength) && i + uiLength == size)
        {
            outLength = (uint)(size - i);
            return true;
        }
    }
    return false;
}

CTextureItem* CEmojiManager::TryMatch(const wchar_t* pText, uint& outLength) const
{
    // Unicode code point, possibly encoded as a UTF-16 surrogate pair
    wchar_t high = pText[0];
    if (high >= 0xd800 && high <= 0xdbff && pText[1] >= 0xdc00 && pText[1] <= 0xdfff)
    {
        char32_t codepoint = 0x10000 + (((char32_t)high - 0xd800) << 10) + (pText[1] - 0xdc00);
        auto     it = m_CodepointMap.find(codepoint);
        if (it != m_CodepointMap.end())
        {
            outLength = 2;
            return it->second;
        }
        return nullptr;
    }

    auto it = m_CodepointMap.find(high);
    if (it != m_CodepointMap.end())
    {
        outLength = 1;
        return it->second;
    }

    // GitHub style :shortcode:
    if (high == L':')
    {
        char szName[32];
        uint uiNameLength = 0;
        while (uiNameLength < NUMELMS(szName) - 1 && pText[1 + uiNameLength] != L'\0' && pText[1 + uiNameLength] != L':')
        {
            wchar_t ch = pText[1 + uiNameLength];
            if (ch > 0x7f)
                return nullptr;
            szName[uiNameLength] = (char)ch;
            uiNameLength++;
        }

        if (uiNameLength > 0 && pText[1 + uiNameLength] == L':')
        {
            szName[uiNameLength] = '\0';
            auto it2 = m_ShortcodeMap.find(szName);
            if (it2 != m_ShortcodeMap.end())
            {
                outLength = uiNameLength + 2;
                return it2->second;
            }
        }
    }

    return nullptr;
}
