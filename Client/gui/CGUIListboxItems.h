/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIListboxItems.h
 *  PURPOSE:     Listbox item types CEGUI 0.4 provided but 0.8.7 dropped
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <CEGUI/widgets/ListboxTextItem.h>

// CEGUI 0.4's base ListboxItem carried its own separate sort text (falling back to the display
// text when none was set) and compared items by that instead of the display text. CEGUI 0.8.7
// dropped this entirely, but MTA's guiGridListSetItemText script function still lets scripts set
// a distinct sort key per cell, so we bring that behaviour back here.
class CGUIListboxTextItem : public CEGUI::ListboxTextItem
{
public:
    CGUIListboxTextItem(const CEGUI::String& text, CEGUI::uint itemId = 0, void* itemData = nullptr, bool disabled = false, bool autoDelete = true) :
        CEGUI::ListboxTextItem(text, itemId, itemData, disabled, autoDelete)
    {
    }

    void SetSortText(const CEGUI::String& strSortText) { m_strSortText = strSortText; }

    const CEGUI::String& GetSortText() const { return m_strSortText.empty() ? getText() : m_strSortText; }

    bool operator<(const CEGUI::ListboxItem& rhs) const override
    {
        if (auto* pOther = dynamic_cast<const CGUIListboxTextItem*>(&rhs))
            return GetSortText() < pOther->GetSortText();
        return getText() < rhs.getText();
    }

    bool operator>(const CEGUI::ListboxItem& rhs) const override
    {
        if (auto* pOther = dynamic_cast<const CGUIListboxTextItem*>(&rhs))
            return GetSortText() > pOther->GetSortText();
        return getText() > rhs.getText();
    }

private:
    CEGUI::String m_strSortText;
};

// Replaces CEGUI 0.4's ListboxNumberItem (removed in 0.8.7): a text item that sorts by the
// numeric value of its sort text rather than lexicographically.
class CGUIListboxNumberItem : public CGUIListboxTextItem
{
public:
    CGUIListboxNumberItem(const CEGUI::String& text, CEGUI::uint itemId = 0, void* itemData = nullptr, bool disabled = false, bool autoDelete = true) :
        CGUIListboxTextItem(text, itemId, itemData, disabled, autoDelete)
    {
    }

    bool operator<(const CEGUI::ListboxItem& rhs) const override { return atoi(GetSortText().c_str()) < atoi(rhs.getText().c_str()); }
    bool operator>(const CEGUI::ListboxItem& rhs) const override { return atoi(GetSortText().c_str()) > atoi(rhs.getText().c_str()); }
};

// Replaces CEGUI 0.4's ListboxImageItem (removed in 0.8.7): a listbox item that renders a single
// image instead of text, sorted by image identity like the old class was.
class CGUIListboxImageItem : public CEGUI::ListboxItem
{
public:
    CGUIListboxImageItem(const CEGUI::Image* pImage, CEGUI::uint itemId = 0, void* itemData = nullptr, bool disabled = false, bool autoDelete = true) :
        CEGUI::ListboxItem("", itemId, itemData, disabled, autoDelete), m_pImage(pImage)
    {
    }

    const CEGUI::Image* GetImage() const { return m_pImage; }
    void                 SetImage(const CEGUI::Image* pImage) { m_pImage = pImage; }

    CEGUI::Sizef getPixelSize(void) const override { return m_pImage ? m_pImage->getRenderedSize() : CEGUI::Sizef(0.0f, 0.0f); }

    void draw(CEGUI::GeometryBuffer& buffer, const CEGUI::Rectf& targetRect, float alpha, const CEGUI::Rectf* clipper) const override
    {
        if (isSelected() && getSelectionBrushImage())
            getSelectionBrushImage()->render(buffer, targetRect, clipper, getModulateAlphaColourRect(getSelectionColours(), alpha));

        if (m_pImage)
            m_pImage->render(buffer, targetRect, clipper, CEGUI::ColourRect(CEGUI::Colour(1.0f, 1.0f, 1.0f, alpha)));
    }

    bool operator<(const CEGUI::ListboxItem& rhs) const override
    {
        auto* pOther = dynamic_cast<const CGUIListboxImageItem*>(&rhs);
        return m_pImage < (pOther ? pOther->GetImage() : nullptr);
    }

    bool operator>(const CEGUI::ListboxItem& rhs) const override
    {
        auto* pOther = dynamic_cast<const CGUIListboxImageItem*>(&rhs);
        return m_pImage > (pOther ? pOther->GetImage() : nullptr);
    }

private:
    const CEGUI::Image* m_pImage;
};
