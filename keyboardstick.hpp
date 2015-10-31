/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: keyboardstick.hpp,v 1.8 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: An emulation layer for keyboard driven joysticks
 **********************************************************************************/

#ifndef KEYPADSTICK_HPP
#define KEYPADSTICK_HPP

/// Includes
#include "string.hpp"
#include "gameport.hpp"
#include "configurable.hpp"
///

/// Forwards
class Machine;
///

/// Class Keyboardstick
class KeyboardStick : public GamePort, public Configurable {
  //
public:
  // Since the keyboard stick works independent from the graphical front-
  // end, the following "pseudo-keycodes" have to be used to code special
  // keys that have their own key-syms in the SDL resp. in X11.
  enum KeyName {
    ArrowLeft  = 0x100,    // cursor keys
    ArrowRight,
    ArrowUp,
    ArrowDown,
    Return,                // Return on the main keyboard
    Tab,                   // On the main keyboard
    Backspace,
    KP_0,                  // Keypad
    KP_1,
    KP_2,
    KP_3,
    KP_4,
    KP_5,
    KP_6,
    KP_7,
    KP_8,
    KP_9,
    KP_Divide,
    KP_Times,
    KP_Minus,
    KP_Plus,
    KP_Enter,
    KP_Digit,               // decimal separator, dot or comma
    SP_Insert,              // Special keys on top of cursor keys
    SP_Delete,
    SP_Home,
    SP_End,
    SP_ScrollUp,
    SP_ScrollDown,
    Count                  // Number of entries here.
  };
private:
  //
  // The following structure associates a Atari++ specific keycode
  // with a front-end specific key-code, or up to three of them.
  struct KeyAssociate {
    int Keycode;           // the atari++ keycode from the list above.
    int Key1,Key2,Key3;    // The associate front-end specific key.
  };
  //
  // The association list for each specific key.
  struct KeyAssociate TranslationTable[Count - 0x100];
  //
  // The keyboard codes (internal codes) for the joystick functions.
  int DirectionCodes[3][3];
  int ButtonCodes[2];
  //
  // Names of the above.
  char *DirectionNames[3][3];
  char *ButtonNames[2];
  //
  // Current keystate encoding for the 3x3 buttons that
  // emulate the joystick
  bool states[3][3];
  bool button[2];
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
  void KeypadFire(bool down,int idx)
  {
    button[idx] = down;
  }
  //
  // Convert the current settings to strings.
  void CodesToNames(void);
  //
  // Convert a single entry in the table from a code to a string
  void CodeToName(char *&name,int code);
  //
  //
public:
  // Constructor: This input class is called "KeypadStick"
  KeyboardStick(class Machine *mach);
  //
  //
  ~KeyboardStick(void);
  //  
  // Reset the keypad stick here by cleaning all states
  void Reset(void)
  {
    memset(states,0,sizeof(states));
    memset(button,0,sizeof(button));
  }
  //
  // Transmit the current state to the game controllers that are
  // part of this chain.
  void TransmitStates(bool paused);
  //
  // Return the name of a key given its key-code.
  static const char *KeyName(int keycode);
  //
  // Convert a keyboard key back into a keyboard code.
  // Returns zero for invalid names.
  static int KeyCode(char *name);
  //
  // Associate an Atari++ specific keyboard code with a front-end specific code.
  void AssociateKey(enum KeyName name,int frontcode);
  //
  // For two alternative front-end codes.
  void AssociateKey(enum KeyName name,int frontcode1,int frontcode2);
  //
  // For three alternative front-end codes.
  void AssociateKey(enum KeyName name,int frontcode1,int frontcode2,int frontcode3);
  //
  // Define the joystick keyboard codes. Note that this requires the Atari++ keycodes.
  // First define the keyboard code for the indicated joystick direction.
  void DefineDirectionKey(int dx,int dy,int internalcode);
  //
  // Second define the keys for the buttons.
  void DefineButtonKey(int button,int internalcode);
  //
  // Check whether the given key code (front-end specific) is a joystick type event.
  // If so, handle it here and generate the button or stick movement, then return true.
  // Otherwise, return false.
  bool HandleJoystickKeys(bool updown,int frontcode);
  //
  // Parse the arguments off
  virtual void ParseArgs(class ArgParser *args);
};
///

///
#endif

