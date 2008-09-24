/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: keyboardstick.cpp,v 1.2 2003/07/22 21:45:13 thor Exp $
 **
 ** In this module: An emulation layer for keyboard driven joysticks
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "keyboardstick.hpp"
///

/// KeyboardStick::TransmitStates
// Transmit the current state to the game controllers that are
// part of this chain.
void KeyboardStick::TransmitStates(bool paused)
{
  WORD dx,dy;
  //
  if (paused) {
    FeedAnalog(0,0);
    FeedButton(false);
  } else {
    // Assume default settings, now.
    dx = 0,dy = 0;
    //
    if (states[0][0]|states[0][1]|states[0][2]) dx = -32767; // "leftish" movement
    if (states[2][0]|states[2][1]|states[2][2]) dx =  32767; // "rightish" movement
    if (states[0][0]|states[1][0]|states[2][0]) dy = -32767; // "upish" movement
    if (states[0][2]|states[1][2]|states[2][2]) dy =  32767; // "downish" movement
    //
    FeedAnalog(dx,dy);
    FeedButton(button);
  }
}
///
