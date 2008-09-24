/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: keyboardstick.hpp,v 1.3 2003/07/22 21:45:13 thor Exp $
 **
 ** In this module: An emulation layer for keyboard driven joysticks
 **********************************************************************************/

#ifndef KEYPADSTICK_HPP
#define KEYPADSTICK_HPP

/// Includes
#include "string.hpp"
#include "gameport.hpp"
///

/// Forwards
class Machine;
///

/// Class Keyboardstick
class KeyboardStick : public GamePort {
  //
  // Current keystate encoding for the 3x3 buttons that
  // emulate the joystick
  bool states[3][3];
  bool button;
  //
public:
  // Constructor: This input class is called "KeypadStick"
  KeyboardStick(class Machine *mach)
    : GamePort(mach,"KeypadStick",0), button(false)
  { 
    memset(states,0,sizeof(states));
  }
  //
  ~KeyboardStick(void)
  { }
  //  
  // Reset the keypad stick here by cleaning all states
  void Reset(void)
  {
    memset(states,0,sizeof(states));
  }
  //
  // Update the state of the keyboard
  void KeypadMove(bool down,int dx,int dy)
  {
    states[dx+1][dy+1] = down;
    // In case we press the middle key, reset all states
    if (dx == 0 && dy == 0 && down) {
      memset(states,0,sizeof(states));
    }
  }
  // Just transmit data to the input chain here.
  void KeypadFire(bool down)
  {
    button = down;
  }
  //
  // Transmit the current state to the game controllers that are
  // part of this chain.
  void TransmitStates(bool paused);
};
///

///
#endif

