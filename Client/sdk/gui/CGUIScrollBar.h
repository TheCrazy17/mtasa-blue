/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/gui/CGUIScrollBar.h
 *  PURPOSE:     Scroll bar widget interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "CGUIElement.h"
#include "CGUICallback.h"

class CGUIScrollBar : public CGUIElement
{
public:
    virtual ~CGUIScrollBar() {};

    virtual void SetOnScrollHandler(const GUI_CALLBACK& Callback) = 0;

    virtual void  SetScrollPosition(float fPosition) = 0;
    virtual float GetScrollPosition() = 0;

    virtual void  SetThumbDynamic(bool bDynamic) = 0;
    virtual bool  GetThumbDynamic() = 0;

    virtual void  SetScrollBarThumbSize(float fSize) = 0;
    virtual float GetScrollBarThumbSize() = 0;

    virtual void  SetScrollBarDocumentSize(float fSize) = 0;
    virtual float GetScrollBarDocumentSize() = 0;

    virtual void  SetScrollBarPageSize(float fSize) = 0;
    virtual float GetScrollBarPageSize() = 0;
};
