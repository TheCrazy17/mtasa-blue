/************************************************************************
    filename:   FalScrollbar.cpp
    created:    Mon Jul 4 2005
    author:     Paul D Turner <paul@cegui.org.uk>
*************************************************************************/
/*************************************************************************
    Crazy Eddie's GUI System (http://www.cegui.org.uk)
    Copyright (C)2004 - 2005 Paul D Turner (paul@cegui.org.uk)
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
 
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*************************************************************************/
#include "StdInc.h"
#include "FalScrollbar.h"
#include "falagard/CEGUIFalWidgetLookManager.h"
#include "falagard/CEGUIFalWidgetLookFeel.h"
#include "CEGUIWindowManager.h"
#include "elements/CEGUIThumb.h"
#include "elements/CEGUIPushButton.h"

// Start of CEGUI namespace section
namespace CEGUI
{
    const utf8 FalagardScrollbar::WidgetTypeName[] = "Falagard/Scrollbar";
    FalagardScrollbarProperties::VerticalScrollbar FalagardScrollbar::d_verticalProperty;
    FalagardScrollbarProperties::DynamicThumb      FalagardScrollbar::d_dynamicThumbProperty;
    FalagardScrollbarProperties::ThumbSize         FalagardScrollbar::d_thumbSizeProperty;


    FalagardScrollbar::FalagardScrollbar(const String& type, const String& name) :
        Scrollbar(type, name),
        d_vertical(false),
        d_dynamicThumb(false),
        d_thumbSize(-1.0f)
    {
        addProperty(&d_verticalProperty);
        addProperty(&d_dynamicThumbProperty);
        addProperty(&d_thumbSizeProperty);
    }

    FalagardScrollbar::~FalagardScrollbar()
    {
    }

    void FalagardScrollbar::populateRenderCache()
    {
        const StateImagery* imagery;

        // get WidgetLookFeel for the assigned look.
        const WidgetLookFeel& wlf = WidgetLookManager::getSingleton().getWidgetLook(d_lookName);
        // try and get imagery for our current state
        imagery = &wlf.getStateImagery(isDisabled() ? "Disabled" : "Enabled");
        // peform the rendering operation.
        imagery->render(*this);
    }

    Thumb* FalagardScrollbar::createThumb(const String& name) const
    {
        // return thumb created by look'n'feel assignment.
        return static_cast<Thumb*>(WindowManager::getSingleton().getWindow(name));
    }

    PushButton* FalagardScrollbar::createIncreaseButton(const String& name) const
    {
        // return button created by look'n'feel assignment.
        return static_cast<PushButton*>(WindowManager::getSingleton().getWindow(name));
    }

    PushButton* FalagardScrollbar::createDecreaseButton(const String& name) const
    {
        // return button created by look'n'feel assignment.
        return static_cast<PushButton*>(WindowManager::getSingleton().getWindow(name));
    }

    void FalagardScrollbar::performChildWindowLayout()
    {
        Scrollbar::performChildWindowLayout();
        updateThumb();
    }

    void FalagardScrollbar::updateThumb(void)
    {
        const WidgetLookFeel& wlf = WidgetLookManager::getSingleton().getWidgetLook(d_lookName);
        Rect area(wlf.getNamedArea("ThumbTrackArea").getArea().getPixelRect(*this));

        Thumb* theThumb = static_cast<Thumb*>(WindowManager::getSingleton().getWindow(getName() + "__auto_thumb__"));

        float posExtent = d_documentSize - d_pageSize;
        float slideExtent;

        if (d_vertical)
        {
            float trackLength = area.getHeight();
            float thumbHeight;

            if (d_dynamicThumb && d_documentSize > 0.0f)
                thumbHeight = (d_pageSize / d_documentSize) * trackLength;
            else if (d_thumbSize >= 0.0f)
                thumbHeight = d_thumbSize * trackLength;
            else
                thumbHeight = theThumb->getAbsoluteHeight();

            if (d_dynamicThumb || d_thumbSize >= 0.0f)
            {
                // Enforce a sensible minimum so the thumb stays grabbable.
                float minSize = std::min(16.0f, trackLength * 0.05f);
                thumbHeight = std::max(thumbHeight, minSize);
                thumbHeight = std::min(thumbHeight, trackLength);
                theThumb->setHeight(absoluteToRelativeY_impl(this, thumbHeight));
            }

            slideExtent = trackLength - thumbHeight;
            theThumb->setVertRange(absoluteToRelativeY_impl(this, area.d_top), absoluteToRelativeY_impl(this, area.d_top + slideExtent));
            theThumb->setPosition(Point(absoluteToRelativeX_impl(this, area.d_left),
                absoluteToRelativeY_impl(this, area.d_top + (posExtent > 0.0f ? d_position * (slideExtent / posExtent) : 0.0f))));
        }
        else
        {
            float trackLength = area.getWidth();
            float thumbWidth;

            if (d_dynamicThumb && d_documentSize > 0.0f)
                thumbWidth = (d_pageSize / d_documentSize) * trackLength;
            else if (d_thumbSize >= 0.0f)
                thumbWidth = d_thumbSize * trackLength;
            else
                thumbWidth = theThumb->getAbsoluteWidth();

            if (d_dynamicThumb || d_thumbSize >= 0.0f)
            {
                float minSize = std::min(16.0f, trackLength * 0.05f);
                thumbWidth = std::max(thumbWidth, minSize);
                thumbWidth = std::min(thumbWidth, trackLength);
                theThumb->setWidth(absoluteToRelativeX_impl(this, thumbWidth));
            }

            slideExtent = trackLength - thumbWidth;
            theThumb->setHorzRange(absoluteToRelativeX_impl(this, area.d_left), absoluteToRelativeX_impl(this, area.d_left + slideExtent));
            theThumb->setPosition(Point(absoluteToRelativeX_impl(this, area.d_left + (posExtent > 0.0f ? d_position * (slideExtent / posExtent) : 0.0f)),
                absoluteToRelativeY_impl(this, area.d_top)));
        }
    }

    float FalagardScrollbar::getValueFromThumb(void) const
    {
        const WidgetLookFeel& wlf = WidgetLookManager::getSingleton().getWidgetLook(d_lookName);
        Rect area(wlf.getNamedArea("ThumbTrackArea").getArea().getPixelRect(*this));

        Thumb* theThumb = static_cast<Thumb*>(WindowManager::getSingleton().getWindow(getName() + "__auto_thumb__"));
        float posExtent = d_documentSize - d_pageSize;

        if (d_vertical)
        {
            float slideExtent = area.getHeight() - theThumb->getAbsoluteHeight();
            return (theThumb->getAbsoluteYPosition() - area.d_top) / (slideExtent / posExtent);
        }
        else
        {
            float slideExtent = area.getWidth() - theThumb->getAbsoluteWidth();
            return (theThumb->getAbsoluteXPosition() - area.d_left) / (slideExtent / posExtent);
        }
    }

    float FalagardScrollbar::getAdjustDirectionFromPoint(const Point& pt) const
    {
        Rect absrect(WindowManager::getSingleton().getWindow(getName() + "__auto_thumb__")->getUnclippedPixelRect());

        if ((d_vertical && (pt.d_y > absrect.d_bottom)) ||
            (!d_vertical && (pt.d_x > absrect.d_right)))
        {
            return 1;
        }
        else if ((d_vertical && (pt.d_y < absrect.d_top)) ||
            (!d_vertical && (pt.d_x < absrect.d_left)))
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }

    bool FalagardScrollbar::isVertical() const
    {
        return d_vertical;
    }

    void FalagardScrollbar::setVertical(bool setting)
    {
        d_vertical = setting;
    }

    bool FalagardScrollbar::isThumbDynamic() const
    {
        return d_dynamicThumb;
    }

    void FalagardScrollbar::setThumbDynamic(bool setting)
    {
        d_dynamicThumb = setting;
        updateThumb();
    }

    float FalagardScrollbar::getThumbSize() const
    {
        return d_thumbSize;
    }

    void FalagardScrollbar::setThumbSize(float size)
    {
        d_thumbSize = size;
        updateThumb();
    }

    //////////////////////////////////////////////////////////////////////////
    /*************************************************************************

        Factory Methods

    *************************************************************************/
    //////////////////////////////////////////////////////////////////////////
    Window* FalagardScrollbarFactory::createWindow(const String& name)
    {
        return new FalagardScrollbar(d_type, name);
    }

    void FalagardScrollbarFactory::destroyWindow(Window* window)
    {
        delete window;
    }

} // End of  CEGUI namespace section
