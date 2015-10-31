/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: display.cpp,v 1.8 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Interface definition for the visual display frontend
 **********************************************************************************/

/// Includes
#include "machine.hpp"
#include "chip.hpp"
#include "display.hpp"
///

/// AtariDisplay::AtariDisplay
AtariDisplay::AtariDisplay(class Machine *mach,int unit)
  : Chip(mach,unit?"XEPDisplay":"Display"), VBIAction(mach), Unit(unit)
{
}
///

/// AtariDisplay::~AtariDisplay
AtariDisplay::~AtariDisplay(void)
{
}
///

/// AtariDisplay::MenuVerify
// Test whether the user requests the menu by pointing the mouse into
// the menu bar and holding the button. Returns true if so.
bool AtariDisplay::MenuVerify(void)
{
  // Only if we have a mouse. This is a virtual method
  // supplied by the derived classes.
  if (MouseIsAvailable()) {
    LONG x,y;
    bool button;
    // The mouse is not in use. We may hijack it to
    // test for the quick menu.
    MousePosition(x,y,button);
    if (button) {
      LONG le,te,w,h,m;
      // Check whether all this happens within the display.
      BufferDimensions(le,te,w,h,m);
      if (x >= le && x < le + w && y >= te && y < te + h) {
	return true;
      }
    }
  }
  return false;
}
///
