/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUILabel_Impl.cpp
 *  PURPOSE:     Label widget class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

#define CGUILABEL_NAME "CGUI/StaticText"

CGUILabel_Impl::CGUILabel_Impl(CGUI_Impl* pGUI, CGUIElement* pParent, const char* szText)
{
    SetManager(pGUI);

    // Get an unique identifier for CEGUI (gah, there's gotta be an another way)
    char szUnique[CGUI_CHAR_SIZE];
    pGUI->GetUniqueName(szUnique);

    // Create the window and set default settings
    m_pWindow = pGUI->GetWindowManager()->createWindow(CGUILABEL_NAME, szUnique);
    m_pWindow->setDestroyedByParent(false);

    // Store the pointer to this CGUI element in the CEGUI element
    m_pWindow->setUserData(reinterpret_cast<void*>(this));

    AddEvents();

    // Do some hardcore disabling on the labels
    // m_pWindow->moveToBack ( );
    // m_pWindow->disable ( );

    // not sure what that was for, disabled
    // m_pWindow->setZOrderingEnabled ( false );
    // m_pWindow->setAlwaysOnTop ( true );

    SetFrameEnabled(false);
    SetHorizontalAlign(CGUI_ALIGN_LEFT);
    SetVerticalAlign(CGUI_ALIGN_TOP);
    SetText(szText);
    GetStaticText()->setBackgroundEnabled(false);

    // If a parent is specified, add it to it's children list, if not, add it as a child to the pManager
    if (pParent)
    {
        SetParent(pParent);
    }
    else
    {
        pGUI->AddChild(this);
        SetParent(NULL);
    }
}

CGUILabel_Impl::~CGUILabel_Impl()
{
    DestroyElement();
}

void CGUILabel_Impl::SetText(const char* Text)
{
    // Set the new text and size the text field after it
    m_pWindow->setText(CGUI_Impl::GetUTFString(Text));
}

void CGUILabel_Impl::SetVerticalAlign(CGUIVerticalAlign eAlign)
{
    // CEGUI 0.8.7's VerticalTextFormatting enum orders Bottom/Centre differently than our own
    // enum (which mirrors the old CEGUI 0.4 ordering), so this needs an explicit mapping rather
    // than a positional cast.
    CEGUI::VerticalTextFormatting eFormatting = CEGUI::VTF_TOP_ALIGNED;
    switch (eAlign)
    {
        case CGUI_ALIGN_BOTTOM:
            eFormatting = CEGUI::VTF_BOTTOM_ALIGNED;
            break;
        case CGUI_ALIGN_VERTICALCENTER:
            eFormatting = CEGUI::VTF_CENTRE_ALIGNED;
            break;
        default:
            eFormatting = CEGUI::VTF_TOP_ALIGNED;
            break;
    }
    GetStaticText()->setVerticalFormatting(eFormatting);
}

CGUIVerticalAlign CGUILabel_Impl::GetVerticalAlign()
{
    switch (GetStaticText()->getVerticalFormatting())
    {
        case CEGUI::VTF_BOTTOM_ALIGNED:
            return CGUI_ALIGN_BOTTOM;
        case CEGUI::VTF_CENTRE_ALIGNED:
            return CGUI_ALIGN_VERTICALCENTER;
        default:
            return CGUI_ALIGN_TOP;
    }
}

void CGUILabel_Impl::SetHorizontalAlign(CGUIHorizontalAlign eAlign)
{
    // Unlike the vertical formatting enum, CEGUI 0.8.7's HorizontalTextFormatting values line up
    // positionally with our own enum, so a direct cast is safe here.
    GetStaticText()->setHorizontalFormatting(static_cast<CEGUI::HorizontalTextFormatting>(eAlign));
}

CGUIHorizontalAlign CGUILabel_Impl::GetHorizontalAlign()
{
    return static_cast<CGUIHorizontalAlign>(GetStaticText()->getHorizontalFormatting());
}

void CGUILabel_Impl::SetTextColor(CGUIColor Color)
{
    GetStaticText()->setTextColours(CEGUI::ColourRect(CEGUI::Colour(1.0f / 255.0f * Color.R, 1.0f / 255.0f * Color.G, 1.0f / 255.0f * Color.B)));
}

void CGUILabel_Impl::SetTextColor(unsigned char ucRed, unsigned char ucGreen, unsigned char ucBlue)
{
    GetStaticText()->setTextColours(CEGUI::ColourRect(CEGUI::Colour(1.0f / 255.0f * ucRed, 1.0f / 255.0f * ucGreen, 1.0f / 255.0f * ucBlue)));
}

CGUIColor CGUILabel_Impl::GetTextColor()
{
    CGUIColor temp;
    GetTextColor(temp.R, temp.G, temp.B);
    return temp;
}

void CGUILabel_Impl::GetTextColor(unsigned char& ucRed, unsigned char& ucGreen, unsigned char& ucBlue)
{
    CEGUI::Colour r = GetStaticText()->getTextColours().getColourAtPoint(0, 0);

    ucRed = (unsigned char)(r.getRed() * 255);
    ucGreen = (unsigned char)(r.getGreen() * 255);
    ucBlue = (unsigned char)(r.getBlue() * 255);
}

void CGUILabel_Impl::SetFrameEnabled(bool bFrameEnabled)
{
    GetStaticText()->setFrameEnabled(bFrameEnabled);
}

bool CGUILabel_Impl::IsFrameEnabled()
{
    return GetStaticText()->isFrameEnabled();
}

float CGUILabel_Impl::GetCharacterWidth(int iCharIndex)
{
    if (true)
        return true;
}

float CGUILabel_Impl::GetFontHeight()
{
    const CEGUI::Font* pFont = m_pWindow->getFont();
    if (pFont)
        return pFont->getFontHeight();
    return 14.0f;
}

float CGUILabel_Impl::GetTextExtent()
{
    const CEGUI::Font* pFont = m_pWindow->getFont();
    if (pFont)
    {
        try
        {
            // Retrieve the longest line's extent
            std::stringstream ssText(m_pWindow->getText().c_str());
            std::string       sLineText;
            float             fMax = 0.0f, fLineExtent = 0.0f;

            while (std::getline(ssText, sLineText))
            {
                fLineExtent = pFont->getTextExtent(CGUI_Impl::GetUTFString(sLineText));
                if (fLineExtent > fMax)
                    fMax = fLineExtent;
            }
            return fMax;
        }
        catch (CEGUI::Exception e)
        {
        }
    }

    return 0.0f;
}
