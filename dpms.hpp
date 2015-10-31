/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: dpms.hpp,v 1.3 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: DPMS interfaces for X11
 **********************************************************************************/

#ifndef DMPS_HPP
#define DMPS_HPP

#include "types.h"
#ifndef X_DISPLAY_MISSING
#include <X11/X.h>
#include <X11/Xlib.h>
#if defined(HAS_DPMS_H) && defined(HAVE_X11_EXTENSIONS_DPMS_H) && defined(HAVE_X11_EXTENSIONS_XEXT_H)
#define USE_DPMS
#endif

void disableDPMS(Display *display,bool really);
void enableDPMS(Display *display);

#endif
#endif
