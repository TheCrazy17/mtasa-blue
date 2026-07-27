/***********************************************************************
    created:    27/7/2026
*************************************************************************/
// Scheme::loadWindowFactories() calls this directly when CEGUI_STATIC is defined, regardless of
// whether any scheme actually declares a <WindowSet> entry. MTA's own schemes never do (every
// window type used comes from CEGUI's built-in widgets, which self-register on their own), so
// this only needs to exist to satisfy the linker; an empty module is never asked to register
// anything in practice.
#include "CEGUI/FactoryModule.h"

//----------------------------------------------------------------------------//
extern "C"
CEGUI::FactoryModule& getWindowFactoryModule()
{
    static CEGUI::FactoryModule mod;
    return mod;
}

//----------------------------------------------------------------------------//
