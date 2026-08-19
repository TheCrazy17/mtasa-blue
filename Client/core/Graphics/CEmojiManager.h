/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/Graphics/CEmojiManager.h
 *  PURPOSE:     Header file for emoji texture lookup class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <string>
#include <unordered_map>

// Maps a curated set of Unicode emoji and GitHub-style :shortcode: tokens to small
// inline icon textures. Add a row to g_EmojiTable in CEmojiManager.cpp and drop the
// matching PNG into MTA/cgui/images/emoji to support another emoji.
class CEmojiManager
{
public:
    void LoadTextures(CRenderItemManagerInterface* pRenderItemManager);
    void ReleaseTextures();

    // Quick check used to decide whether a string needs the slower, token-aware draw path.
    bool ContainsTrigger(const std::wstring& wstrText) const;

    // Tries to match an emoji token (Unicode code point or :shortcode:) at pText.
    // On success, returns the texture and sets outLength to the number of wchar_t consumed.
    CTextureItem* TryMatch(const wchar_t* pText, uint& outLength) const;

    // Checks whether wstrText ends with a whole emoji token, so a chat input box can delete it as one
    // unit on backspace instead of unraveling it one wchar_t at a time. Sets outLength on success.
    bool TryMatchAtEnd(const std::wstring& wstrText, uint& outLength) const;

private:
    std::unordered_map<char32_t, CTextureItem*>    m_CodepointMap;
    std::unordered_map<std::string, CTextureItem*> m_ShortcodeMap;
};
