/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIFont_Impl.cpp
 *  PURPOSE:     Font type class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

CGUIFont_Impl::CGUIFont_Impl(CGUI_Impl* pGUI, const char* szFontName, const char* szFontFile, unsigned int uSize, unsigned int uFlags, bool bAutoScale)
{
    // Store the fontmanager and create a font with the given attributes
    m_pFontManager = pGUI->GetFontManager();
    m_pFont = NULL;
    m_pGeometryBuffer = pGUI->GetGeometryBuffer();

    // CEGUI 0.8.7 resolves font files through named resource groups rather than a plain path, and
    // always prefixes the group's directory onto the filename; an "absolute" group with an empty
    // directory lets already-rooted paths (Windows system fonts, full file paths) pass through
    // unmodified instead of getting mangled. Relative paths given here (e.g. "cgui/sagothic.ttf")
    // are always MTA's own bundled fonts, resolved against the MTA folder through "mta-fonts", not
    // the skin-relative "fonts" group the active skin's own font uses.
    const CEGUI::String strResourceGroup = SharedUtil::IsAbsolutePath(szFontFile) ? "absolute" : "mta-fonts";
    const CEGUI::AutoScaledMode eScaleMode = bAutoScale ? CEGUI::ASM_Both : CEGUI::ASM_Disabled;

    while (!m_pFont)
    {
        try
        {
            m_pFont = &m_pFontManager->createFreeTypeFont(CGUI_Impl::GetUTFString(szFontName), static_cast<float>(uSize), true, szFontFile, strResourceGroup,
                                                           eScaleMode, CEGUI::Sizef(1024.0f, 768.0f));
        }
        catch (CEGUI::RendererException)
        {
            // Reduce size until it can fit into a texture
            if (--uSize == 1)
                throw;
        }
    }

    // Set default attributes
    SetNativeResolution(1024, 768);
    SetAutoScalingEnabled(bAutoScale);
}

CGUIFont_Impl::~CGUIFont_Impl()
{
    m_pFontManager->destroy(*m_pFont);
}

void CGUIFont_Impl::SetAntiAliasingEnabled(bool bAntialiased)
{
    // CEGUI 0.8.7's FreeType fonts only take anti-aliasing as a one-time creation flag; there is
    // no way to flip it afterwards, so this is a no-op left for interface compatibility.
}

bool CGUIFont_Impl::IsAntiAliasingEnabled()
{
    return true;
}

void CGUIFont_Impl::DrawTextString(const char* szText, CRect2D DrawArea, float fZ, CRect2D ClipRect, unsigned long ulFormat, unsigned long ulColor,
                                   float fScaleX, float fScaleY)
{
    const CEGUI::String strText = szText ? CGUI_Impl::GetUTFString(szText) : CEGUI::String();

    // CEGUI 0.8.7's Font::drawText only takes a draw position, not a destination box with its own
    // alignment mode, so horizontal alignment within DrawArea has to be worked out here first.
    float fX = DrawArea.fX1;
    if (ulFormat == DT_CENTER || ulFormat == DT_RIGHT)
    {
        const float fTextWidth = m_pFont->getTextExtent(strText, fScaleX);
        const float fBoxWidth = DrawArea.fX2 - DrawArea.fX1;
        fX += (ulFormat == DT_CENTER) ? (fBoxWidth - fTextWidth) * 0.5f : (fBoxWidth - fTextWidth);
    }

    const CEGUI::Rectf clipRect(CEGUI::Vector2f(ClipRect.fX1, ClipRect.fY1), CEGUI::Vector2f(ClipRect.fX2, ClipRect.fY2));
    m_pFont->drawText(*m_pGeometryBuffer, strText, CEGUI::Vector2f(fX, DrawArea.fY1), &clipRect, CEGUI::ColourRect(static_cast<CEGUI::argb_t>(ulColor)), 0.0f,
                      fScaleX, fScaleY);
}

void CGUIFont_Impl::SetAutoScalingEnabled(bool bAutoScaled)
{
    m_pFont->setAutoScaled(bAutoScaled ? CEGUI::ASM_Both : CEGUI::ASM_Disabled);
}

bool CGUIFont_Impl::IsAutoScalingEnabled()
{
    return m_pFont->getAutoScaled() != CEGUI::ASM_Disabled;
}

void CGUIFont_Impl::SetNativeResolution(int iX, int iY)
{
    m_pFont->setNativeResolution(CEGUI::Sizef(static_cast<float>(iX), static_cast<float>(iY)));
}

float CGUIFont_Impl::GetCharacterWidth(int iChar, float fScale)
{
    char szBuf[2];
    szBuf[0] = static_cast<char>(iChar);
    szBuf[1] = 0;

    return m_pFont->getTextExtent(szBuf, fScale);
}

float CGUIFont_Impl::GetFontHeight(float fScale)
{
    float fHeight = m_pFont->getFontHeight(fScale);  // average height.. not the maximum height for long characters such as 'g' or 'j'
    fHeight += 2.0f;                                 // so hack it

    return fHeight;
}

float CGUIFont_Impl::GetTextExtent(const char* szText, float fScale)
{
    return m_pFont->getTextExtent(CGUI_Impl::GetUTFString(szText), fScale);
}

CEGUI::Font* CGUIFont_Impl::GetFont()
{
    return m_pFont;
}
