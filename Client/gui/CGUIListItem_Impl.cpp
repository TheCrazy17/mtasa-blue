/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIListItem_Impl.cpp
 *  PURPOSE:     List widget item class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

CGUIListItem_Impl::CGUIListItem_Impl(const char* szText, unsigned int uiType, CGUIStaticImage_Impl* pImage)
{
    ItemType = uiType;

    // Create the requested list item type
    switch (uiType)
    {
        case TextItem:
            m_pListItem = new CGUIListboxTextItem(CGUI_Impl::GetUTFString(szText));
            break;
        case ImageItem:
            m_pListItem = new CGUIListboxImageItem(pImage ? pImage->GetDirectImage() : NULL);
            break;
        case NumberItem:
            m_pListItem = new CGUIListboxNumberItem(CGUI_Impl::GetUTFString(szText));
            break;
    }

    if (m_pListItem)
    {
        // Set flags and properties
        m_pListItem->setAutoDeleted(false);
        m_pListItem->setSelectionBrushImage("CGUI-Images/ListboxSelectionBrush");
    }

    m_pData = NULL;
}

CGUIListItem_Impl::~CGUIListItem_Impl()
{
    if (m_deleteDataCallback)
        m_deleteDataCallback(m_pData);
    delete m_pListItem;
}

void CGUIListItem_Impl::SetDisabled(bool bDisabled)
{
    reinterpret_cast<CEGUI::ListboxItem*>(m_pListItem)->setDisabled(bDisabled);
}

void CGUIListItem_Impl::SetFont(const char* szFontName)
{
    if (szFontName)
        reinterpret_cast<CEGUI::ListboxTextItem*>(m_pListItem)->setFont(CEGUI::String(szFontName));
}

void CGUIListItem_Impl::SetText(const char* pszText, const char* pszSortText)
{
    m_pListItem->setText(CGUI_Impl::GetUTFString(pszText));

    // CEGUI 0.8.7 dropped the separate sort text CEGUI 0.4 had, so we keep our own (see
    // CGUIListboxTextItem in CGUIListboxItems.h).
    if (auto* pTextItem = dynamic_cast<CGUIListboxTextItem*>(m_pListItem))
        pTextItem->SetSortText(pszSortText ? CGUI_Impl::GetUTFString(pszSortText) : CEGUI::String());
}

void CGUIListItem_Impl::SetData(const char* pszData)
{
    if (pszData)
    {
        m_strData = pszData;
        m_pData = (void*)m_strData.c_str();
    }
    else
    {
        m_pData = NULL;
    }
}

void CGUIListItem_Impl::SetImage(CGUIStaticImage* pImage)
{
    if (ItemType == ImageItem)
    {
        CGUIStaticImage_Impl* pImageImpl = (CGUIStaticImage_Impl*)pImage;
        reinterpret_cast<CGUIListboxImageItem*>(m_pListItem)->SetImage(pImageImpl ? pImageImpl->GetDirectImage() : NULL);
    }
}

std::string CGUIListItem_Impl::GetText() const
{
    return CGUI_Impl::GetUTFString(m_pListItem->getText().c_str()).c_str();
}

CEGUI::ListboxItem* CGUIListItem_Impl::GetListItem()
{
    return m_pListItem;
}

bool CGUIListItem_Impl::GetSelectedState()
{
    return m_pListItem->isSelected();
}

void CGUIListItem_Impl::SetSelectedState(bool bState)
{
    m_pListItem->setSelected(bState);
}

void CGUIListItem_Impl::SetColor(unsigned char ucRed, unsigned char ucGreen, unsigned char ucBlue, unsigned char ucAlpha)
{
    if (ItemType == TextItem || ItemType == NumberItem)
    {
        reinterpret_cast<CEGUI::ListboxTextItem*>(m_pListItem)
            ->setTextColours(CEGUI::Colour((float)ucRed / 255.0f, (float)ucGreen / 255.0f, (float)ucBlue / 255.0f, (float)ucAlpha / 255.0f));
    }
}

bool CGUIListItem_Impl::GetColor(unsigned char& ucRed, unsigned char& ucGreen, unsigned char& ucBlue, unsigned char& ucAlpha)
{
    if (ItemType == TextItem || ItemType == NumberItem)
    {
        CEGUI::Colour color = reinterpret_cast<CEGUI::ListboxTextItem*>(m_pListItem)->getTextColours().d_top_left;
        ucRed = static_cast<unsigned char>(color.getRed() * 255);
        ucGreen = static_cast<unsigned char>(color.getGreen() * 255);
        ucBlue = static_cast<unsigned char>(color.getBlue() * 255);
        ucAlpha = static_cast<unsigned char>(color.getAlpha() * 255);
        return true;
    }
    return false;
}
