/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: dpms.cpp,v 1.5 2021/07/03 16:12:51 thor Exp $
 **
 ** In this module: DPMS interfaces for X11
 **********************************************************************************/

#include "dpms.hpp"
#include "types.h"
#ifndef X_DISPLAY_MISSING
// Enforce inclusion of X keymap defiitions
#define XK_MISCELLANY
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef USE_DPMS
extern "C" {
#include <X11/extensions/Xext.h>
#include <X11/extensions/dpms.h>
}
#endif

#ifdef USE_DPMS
static bool statesaved    = false;
static bool enabled       = true;
static int  timeout       = 0;
#endif

#ifdef USE_DPMS
void disableDPMS(Display *display,bool really)
{ 
  int dummy, interval, prefer_blank, allow_exp;

  if (display) {
    if (DPMSQueryExtension(display, &dummy, &dummy)) {
      BOOL onoff   = 0;
      CARD16 state = 0;
      
      DPMSInfo(display,&state,&onoff); 
      if (!statesaved) {
	enabled    = (onoff)?true:false;
	statesaved = true;
	XGetScreenSaver(display,&timeout,&interval,&prefer_blank,&allow_exp);
      }
      if (onoff) {
	if (really) {
	  DPMSDisable(display);
	  XGetScreenSaver(display,  &dummy,&interval,&prefer_blank,&allow_exp);
	  XSetScreenSaver(display,       0, interval, prefer_blank, allow_exp);
	  XGetScreenSaver(display,  &dummy,&interval,&prefer_blank,&allow_exp);
	}
      }
    }
  }
}
#else
void disableDPMS(Display *,bool)
{
}
#endif

#ifdef USE_DPMS
void enableDPMS(Display *display)
{
  int dummy, interval, prefer_blank, allow_exp;
  
  if (display && statesaved) {
    if (DPMSQueryExtension(display,&dummy,&dummy)) { 
      BOOL onoff   = 0;
      CARD16 state = 0;
      if (enabled) {
	DPMSEnable(display);
	DPMSForceLevel(display, DPMSModeOn);
      }
      // Must run into info to force the level...
      DPMSInfo(display, &state, &onoff);
      XGetScreenSaver(display,  &dummy,&interval,&prefer_blank,&allow_exp);
      XSetScreenSaver(display, timeout, interval, prefer_blank, allow_exp);
      XGetScreenSaver(display,  &dummy,&interval,&prefer_blank,&allow_exp);
      statesaved = false;
    }
  }
}
#else
void enableDPMS(Display *)
{
}
#endif


/// Force DPMS level to on
#ifdef USE_DPMS
void enableMonitor(Display *display)
{
  DPMSForceLevel(display,DPMSModeOn);
}
#else
void enableMonitor(Display *)
{
}
#endif
///

#endif
