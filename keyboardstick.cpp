/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: keyboardstick.cpp,v 1.10 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: An emulation layer for keyboard driven joysticks
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "keyboardstick.hpp"
#include "exceptions.hpp"
///

/// KeyboardStick::KeyName
// Return the name of a key given its key-code.
const char *KeyboardStick::KeyName(int keycode)
{
  static char buf[2];
  
  if ((keycode >= '0' && keycode <= '9') ||
      (keycode >= 'A' && keycode <= 'Z')) {
    buf[0] = char(keycode);
    buf[1] = 0;
    return buf;
  } else if ((keycode >= 'a' && keycode <= 'z')) {
    buf[0] = char(keycode - 'a' + 'A');
    buf[1] = 0;
    return buf;
  } else switch(keycode) {
    case ' ':
      return "Space";
    case ArrowLeft:
      return "Cursor Left";
    case ArrowRight:
      return "Cursor Right";
    case ArrowUp:
      return "Cursor Up";
    case ArrowDown:
      return "Cursor Down";
    case Return:
      return "Return";
    case Tab:
      return "Tab";
    case Backspace:
      return "Backspace";
    case KP_0:
      return "Keypad 0";
    case KP_1:
      return "Keypad 1";
    case KP_2:
      return "Keypad 2";
    case KP_3:
      return "Keypad 3";
    case KP_4:
      return "Keypad 4";
    case KP_5:
      return "Keypad 5";
    case KP_6:
      return "Keypad 6";
    case KP_7:
      return "Keypad 7";
    case KP_8:
      return "Keypad 8";
    case KP_9:
      return "Keypad 9";
    case KP_Divide:
      return "Keypad Divide";
    case KP_Times:
      return "Keypad Multiply";
    case KP_Minus:
      return "Keypad Minus";
    case KP_Plus:
      return "Keypad Plus";
    case KP_Enter:
      return "Keypad Enter";
    case KP_Digit:
      return "Keypad Dot";
    case SP_Insert:
      return "Insert";
    case SP_Delete:
      return "Delete";
    case SP_Home:
      return "Home";
    case SP_End:
      return "End";
    case SP_ScrollUp:
      return "Scroll Up";
    case SP_ScrollDown:
      return "Scroll Down";
  }
  //
  // Not defined. Return an error.
  return NULL;
}
///

/// KeyboardStick::KeyCode
// Convert a keyboard key back into a keyboard code.
// Returns zero for invalid names.
int KeyboardStick::KeyCode(char *name)
{
  int i;
  
  if (strlen(name) == 1) {
    if (*name == ' ') {
      strcpy(name,"Space");
      return ' ';
    }
    if (*name >= '0' && *name <= '9')
      return *name;
    if (*name >= 'A' && *name <= 'Z')
      return *name;
    if (*name >= 'a' && *name <= 'z') {
      *name += 'A' - 'a';
      return *name ;
    }
  }
  //
  // All should compare to special strings.
  for(i = ArrowLeft;i < Count;i++) {
    const char *key = KeyName(i);
    if (key) {
      if (!strcasecmp(name,key)) {
	strcpy(name,key);
	return i;
      }
    }
  }
  if (!strcasecmp(name,"Space") || !strcasecmp(name,"Spacebar")) {
    strcpy(name,"Space");
    return ' ';
  }
  //
  // Invalid name.
  return 0;
}
///

/// KeyboardStick::KeyboardStick
KeyboardStick::KeyboardStick(class Machine *mach)
  : GamePort(mach,"KeypadStick",0), Configurable(mach)
{ 
  memset(states,0,sizeof(states));
  memset(button,0,sizeof(button));
  memset(TranslationTable,0,sizeof(TranslationTable));
  memset(DirectionCodes,0,sizeof(DirectionCodes));
  memset(ButtonCodes,0,sizeof(ButtonCodes));  
  //
  // Initialize names.
  memset(DirectionNames,0,sizeof(DirectionNames));
  memset(ButtonNames   ,0,sizeof(ButtonNames));
  //
  // Install default definition.
  DefineDirectionKey(-1,-1,KP_7);
  DefineDirectionKey( 0,-1,KP_8);
  DefineDirectionKey(+1,-1,KP_9);
  DefineDirectionKey(-1, 0,KP_4);
  DefineDirectionKey( 0, 0,KP_5);
  DefineDirectionKey(+1, 0,KP_6);
  DefineDirectionKey(-1,+1,KP_1);
  DefineDirectionKey( 0,+1,KP_2);
  DefineDirectionKey(+1,+1,KP_3);
  DefineButtonKey   (0,KP_0);
  DefineButtonKey   (1,KP_Enter);
}
///

/// KeyboardStick::~KeyboardStick
KeyboardStick::~KeyboardStick(void)
{
  int dx,dy,b;

  for(dy = 0;dy <= 2;dy++) {
    for(dx = 0;dx <= 2;dx++) {
      delete DirectionNames[dx][dy];
    }
  }
  for(b = 0;b < 2;b++) {
    delete ButtonNames[b];
  }
}
///

/// KeyboardStick::AssociateKey
// Associate an Atari++ specific keyboard code with a front-end specific code.
void KeyboardStick::AssociateKey(enum KeyName name,int frontcode)
{
  int offset;
  
#if CHECK_LEVEL > 0
  if (name < ArrowLeft || name >= Count)
    Throw(OutOfRange,"KeyboardStick::AssociateKey","keyboard code out of range");
#endif
  offset = name - ArrowLeft;
  TranslationTable[offset].Keycode = name;
  TranslationTable[offset].Key1    = frontcode;
  TranslationTable[offset].Key2    = frontcode;
  TranslationTable[offset].Key3    = frontcode;
}
///

/// KeyboardStick::AssociateKey
// Associate an Atari++ specific keyboard code with a front-end specific code.
void KeyboardStick::AssociateKey(enum KeyName name,int frontcode1,int frontcode2)
{
  int offset;
  
#if CHECK_LEVEL > 0
  if (name < ArrowLeft || name >= Count)
    Throw(OutOfRange,"KeyboardStick::AssociateKey","keyboard code out of range");
#endif
  offset = name - ArrowLeft;
  TranslationTable[offset].Keycode = name;
  TranslationTable[offset].Key1    = frontcode1;
  TranslationTable[offset].Key2    = frontcode2;
  TranslationTable[offset].Key3    = frontcode2;
}
///

/// KeyboardStick::AssociateKey
// Associate an Atari++ specific keyboard code with a front-end specific code.
void KeyboardStick::AssociateKey(enum KeyName name,int frontcode1,int frontcode2,int frontcode3)
{
  int offset;
  
#if CHECK_LEVEL > 0
  if (name < ArrowLeft || name >= Count)
    Throw(OutOfRange,"KeyboardStick::AssociateKey","keyboard code out of range");
#endif
  offset = name - ArrowLeft;
  TranslationTable[offset].Keycode = name;
  TranslationTable[offset].Key1    = frontcode1;
  TranslationTable[offset].Key2    = frontcode2;
  TranslationTable[offset].Key3    = frontcode3;
}
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
    FeedButton(false,0);
    FeedButton(false,1);
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
    FeedButton(button[0],0);
    FeedButton(button[1],1);
  }
}
///

/// KeyboardStick::DefineDirectionKey
// Define the joystick keyboard codes. Note that this requires the Atari++ keycodes.
// First define the keyboard code for the indicated joystick direction.
void KeyboardStick::DefineDirectionKey(int dx,int dy,int internalcode)
{
  dx++,dy++;
#if CHECK_LEVEL > 0
  if (dx < 0 || dx > 2 || dy < 0 || dy > 2)
    Throw(OutOfRange,"KeyboardStick::DefineDirectionKey","joystick direction index out of range");
#endif
  DirectionCodes[dx][dy] = internalcode;
}
///

/// KeyboardStick::DefineButtonKey
// Second define the keys for the buttons.
void KeyboardStick::DefineButtonKey(int button,int internalcode)
{
#if CHECK_LEVEL > 0
  if (button < 0 || button > 1)
    Throw(OutOfRange,"KeyboardStick::DefineButtonKey","joystick button index out of range");
#endif
  ButtonCodes[button] = internalcode;
}
///

/// KeyboardStick::HandleJoystickKeys
// Check whether the given key code (front-end specific) is a joystick type event.
// If so, handle it here and generate the button or stick movement, then return true.
// Otherwise, return false.
bool KeyboardStick::HandleJoystickKeys(bool updown,int frontcode)
{
  int internal = 0;
  int dx,dy,button;
  //
  // Do not eat events if nobody is listening.
  if (ControllerChain().First() == NULL)
    return false;
  //
  // First convert to the internal code.
  if (frontcode < 0x100 && frontcode >= 0x20) {
    internal = frontcode;
    if (internal >= 'a' && internal <= 'z') 
      internal = frontcode + 'A' - 'a';
  } else {
    int i;
    for (i = 0;i < Count - ArrowLeft;i++) {
      if (TranslationTable[i].Key1 == frontcode ||
	  TranslationTable[i].Key2 == frontcode ||
	  TranslationTable[i].Key3 == frontcode) {
	internal = TranslationTable[i].Keycode;
	break;
      }
    }
  }
  //
  // If this is an unknown key, bail out.
  if (internal == 0)
    return false;
  //
  // Now check whether it is a joystick movement.
  for(dy = -1;dy <= 1;dy++) {
    for(dx = -1;dx <= 1;dx++) {
      if (DirectionCodes[dx+1][dy+1] == internal) {
	KeypadMove(updown,dx,dy);
	return true;
      }
    }
  }
  //
  // 
  for(button = 0;button <= 1;button++) {
    if (ButtonCodes[button] == internal) {
      KeypadFire(updown,button);
      return true;
    }
  }
  //
  return false;
}
///

/// KeyboardStick::CodeToName
// Convert a single entry in the table from a code to a string
void KeyboardStick::CodeToName(char *&name,int code)
{ 
  const char *newname;
  
  delete name;
  name = NULL;
  newname = KeyName(code);
  if (newname == NULL)
    newname = "";
  name = new char[strlen(newname) + 1];
  strcpy(name,newname);
}
///

/// KeyboardStick::CodesToNames
// Convert the current settings to strings.
void KeyboardStick::CodesToNames(void)
{
  int dx,dy,b;
  // The following is not very nice at this time, but anyhow...
  // First convert the names to strings.
  for(dy = 0;dy <= 2;dy++) {
    for(dx = 0;dx <= 2;dx++) {
      CodeToName(DirectionNames[dx][dy],DirectionCodes[dx][dy]);
    }
  }
  for(b = 0;b < 2;b++) {
    CodeToName(ButtonNames[b],ButtonCodes[b]);
  }
}
///

/// KeyboardStick::ParseArgs
// Parse the arguments off
void KeyboardStick::ParseArgs(class ArgParser *args)
{
  int dx,dy,b;
  //
  args->DefineTitle("KeypadStick");
  //
  // The following is not very nice at this time, but anyhow...
  // First convert the names to strings.
  CodesToNames();
  //
  // Now get the new arguments.
  args->DefineString("LeftUp","Keyboard button emulating a joystick left-up movement",DirectionNames[0][0]);
  args->DefineString("Up","Keyboard button emulating a joystick upwards movements",DirectionNames[1][0]);
  args->DefineString("RightUp","Keyboard button emulating a joystick right-up movement",DirectionNames[2][0]);
  args->DefineString("Left","Keyboard button emulating a joystick leftwards movement",DirectionNames[0][1]);
  args->DefineString("Center","Keyboard button centering the emulated joystick",DirectionNames[1][1]);
  args->DefineString("Right","Keyboard button emulating a joystick rightwards movement",DirectionNames[2][1]);
  args->DefineString("LeftDown","Keyboard button emulating a joystick left-down movement",DirectionNames[0][2]);
  args->DefineString("Down","Keyboard button emulating a joystick downwards movement",DirectionNames[1][2]);
  args->DefineString("RightDown","Keyboard button emulating a joystick right-down movement",DirectionNames[2][2]);
  //
  args->DefineString("LeftButton","Keyboard button emulating the main joystick button",ButtonNames[0]);
  args->DefineString("RightButton","Keyboard button emulating the 2nd (if any) joystick button",ButtonNames[1]);
  //
  // Now check whether we can find proper key for everything.
  for(dy = 0;dy <= 2;dy++) {
    for(dx = 0;dx <= 2;dx++) {
      if (*DirectionNames[dx][dy] == 0) {
	DirectionCodes[dx][dy] = 0;
      } else {
	int code = KeyCode(DirectionNames[dx][dy]);
	if (code) {
	  DirectionCodes[dx][dy] = code;
	} else {
	  // Fill again with the valid value.
	  CodeToName(DirectionNames[dx][dy],DirectionCodes[dx][dy]);
	  args->PrintError("Key name %s is invalid.",DirectionNames[dx][dy]);
	}
      }
    }
  }
  for(b = 0;b < 2;b++) {
    if (*ButtonNames[b] == 0) {
      ButtonCodes[b] = 0;
    } else {
      int code = KeyCode(ButtonNames[b]);
      if (code) {
	ButtonCodes[b] = code;
      } else {
	CodeToName(ButtonNames[b],ButtonCodes[b]);
	args->PrintError("Key name %s is invalid.",ButtonNames[b]);
      }
    }
  }
}
///
