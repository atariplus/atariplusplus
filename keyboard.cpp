/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: keyboard.cpp,v 1.36 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Definition of the interface towards keyboard input
 **********************************************************************************/

/// Includes
#include "machine.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "keyboard.hpp"
#include "cpu.hpp"
#include "osrom.hpp"
#include "mmu.hpp"
///

/// Keyboard::KeyButtonController::KeyButtonController
Keyboard::KeyButtonController::KeyButtonController(class Machine *mach,char keycode,const char *name)
  : GameControllerNode(mach,1,name,false), Enabled(false), Target(keycode), Button(0)
{
}
///

/// Keyboard::KeyButtonController::~KeyButtonController
Keyboard::KeyButtonController::~KeyButtonController(void)
{
}
///

/// Keyboard::KeyButtonController::ParseArgs
// Implementation of the configurable interface
void Keyboard::KeyButtonController::ParseArgs(class ArgParser *args)
{
  char enable[80];
  char buttonname[80];
  bool oldenable = Enabled;
  //
  // Generate the name for this option.
  snprintf(enable,79,"%s.Enable",DeviceName);
  args->DefineBool(enable,"Disable or enable the generation of key events from joystick buttons",Enabled);
  if (Enabled != oldenable)
    args->SignalBigChange(ArgParser::Reparse);
  if (Enabled) {
    LONG button = Button + 1;
    snprintf(buttonname,79,"%s.Button",DeviceName);
    args->DefineLong(buttonname,"The mapped button index we listen to",1,4,button);
    Button = button - 1;
    GameControllerNode::ParseArgs(args);
  }
}
///

/// Keyboard::KeyButtonController::FeedButton
// Feed a button event from a game port into the controller.
void Keyboard::KeyButtonController::FeedButton(bool value,int button)
{
  if (Enabled && Button == button)
    machine->Keyboard()->HandleSimpleKey(value,Target,false,false);
  //
  // Forward to the parent class.
  GameControllerNode::FeedButton(value,button);
}
///

/// Keyboard::Keycodes
// This table contains the keyboard codes indexed by ASCII characters
// in case the key comes in unshifted
const UBYTE Keyboard::KeyCodes[128] = {
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0x34,0x2c,0x0c,0xff,0xff,0x0c,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0x1c,0xff,0xff,0xff,0xff,

  0x21,0x1f,0x1e,0x1a,0x18,0x1d,0x1b,0x33, 0x30,0x32,0x07,0x06,0x20,0x0e,0x22,0x26,
  0x32,0x1f,0x1e,0x1a,0x18,0x1d,0x1b,0x33, 0x35,0x30,0x02,0x02,0x36,0x0f,0x37,0x26,

  0x35,0x3f,0x15,0x12,0x3a,0x2a,0x38,0x3d, 0x39,0x0d,0x01,0x05,0x00,0x25,0x23,0x08,
  0x0a,0x2f,0x28,0x3e,0x2d,0x0b,0x10,0x2e, 0x16,0x2b,0x17,0x20,0x06,0x22,0x07,0x0e,

  0x33,0x3f,0x15,0x12,0x3a,0x2a,0x38,0x3d, 0x39,0x0d,0x01,0x05,0x00,0x25,0x23,0x08,
  0x0a,0x2f,0x28,0x3e,0x2d,0x0b,0x10,0x2e, 0x16,0x2b,0x17,0xff,0x0f,0xff,0xff,0xff
};
// The table how to interpret the shift key if shift is released pressed.
const UBYTE Keyboard::ShiftCodes[128] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 
  0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01, 0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x01,

  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01, 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,

  0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00
};
// The table for the 5200. No shift key here, this is a separate issue that
// triggers a BREAK but is not included in the keycode
const UBYTE Keyboard::KeyCodes5200[128] = {
   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,

   0xff,0xff,0xff,0x02,0xff,0xff,0xff,0xff, 0xff,0xff,0x06,0xff,0xff,0xff,0xff,0xff,
   0x04,0x1e,0x1c,0x1a,0x16,0x14,0x12,0x0e, 0x0c,0x0a,0xff,0xff,0xff,0xff,0xff,0xff,

   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0x02,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0x06,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,

   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, 0x02,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
   0xff,0xff,0xff,0x06,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};
///

/// Keyboard::Keyboard
Keyboard::Keyboard(class Machine *mach)
  : Chip(mach,"Keyboard"), VBIAction(mach),
    KeyIRQPending(false), BreakIRQPending(false), ResetPending(false),
    KeyDown(false), MenuKeyDown(false), ShiftDown(false), CtrlDown(false),
    Key(0x00), KeyCode(0x3f), ConsoleKeyFlags(0x07),
    KeyUsed(false),
    HoldOption(true), HoldSelect(false), HoldStart(false), TypeAhead(true)
{
}
///

/// Keyboard::~Keyboard
Keyboard::~Keyboard(void)
{
  struct KeyEvent *node;
  class KeyButtonController *ctrl;

  while((node = EventList.RemHead())) {
    delete node;
  }  
  while((node = FreeEvents.RemHead())) {
    delete node;
  }
  while((ctrl = KeyButtonList.RemHead())) {
    delete ctrl;
  }
}
///

/// Keyboard::VBI
// Notification from the timer that a new VBI is active. This "eats" possibly
// read keys.
void Keyboard::VBI(class Timer *,bool,bool)
{
  if (KeyUsed) {
    FeedKey();
    KeyUsed = false;
  }
}
///

/// Keyboard::ColdStart
// Run a keyboard coldstart
void Keyboard::ColdStart(void)
{ 
  struct KeyEvent *node;
 
  ResetCount      = 0;
  if (HoldOption || HoldSelect || HoldStart)
    ResetCount += 2;
  ConsoleKeyFlags = 0x07;  
  KeyIRQPending   = false;
  BreakIRQPending = false;
  KeyDown         = false;
  MenuKeyDown     = false;
  ShiftDown       = false;
  CtrlDown        = false;
  Key             = 0x00;
  KeyCode         = 0x3f;
  KeyUsed         = false;
  //
  while((node = EventList.RemHead())) {
    FreeEvents.AddHead(node);
  }
}
///

/// Keyboard::WarmStart
// Warm-Reset the keyboard
void Keyboard::WarmStart(void)
{ 
  struct KeyEvent *node;
 
  // Try to guess whether this is a coldstart.
  // The reason why we're performing this complicated
  // here is that we should not try to overload the
  // console keys on a warmstart since a program might
  // want to read it here, but we also want a 
  // consistent behaivour in case a simple reset
  // generates a coldstart.
  if (machine->OsROM()->MightColdstart()) {
    // Yes, this will get reverted to a coldstart.
    ResetCount      = 0;
    if (HoldOption || HoldSelect || HoldStart)
      ResetCount += 2;
    ConsoleKeyFlags = 0x07;
  }
  KeyIRQPending   = false;
  BreakIRQPending = false;
  KeyDown         = false;
  MenuKeyDown     = false;
  ShiftDown       = false;
  CtrlDown        = false;
  Key             = 0x00;
  KeyCode         = 0x3f;
  KeyUsed         = false;
  //
  while((node = EventList.RemHead())) {
    FreeEvents.AddHead(node);
  }
}
///

/// Keyboard::ConsoleKeys
// Return the bitmask describing the state of all console keys
UBYTE Keyboard::ConsoleKeys(void)
{
  UBYTE keyflags = ConsoleKeyFlags;
  //
  // Implements the "hold option" functionality, but not for
  // the 5200 series.
  if (machine->MachType() != Mach_5200) {
    if (ResetCount && (machine->CPU()->PC() >= 0xc000)) { // Hacky! Test for ROM resource
      ResetCount--;
      if (HoldOption)
	keyflags &= ~0x04; // hold down option
      if (HoldSelect)
	keyflags &= ~0x02; // hold down select
      if (HoldStart)
	keyflags &= ~0x01; // hold down start
    } else {
      // Non-OsRom read also resets the reset count
      ResetCount = 0;
    }
  }
  return keyflags;
}
///

/// Keyboard::KeyboardStatus
// Return the SKSTAT bitmask for the keyboard state
UBYTE Keyboard::KeyboardStatus(void)
{
  UBYTE bitmask = 0x00;

  if (!ShiftDown)
    bitmask |= 0x08;
  
  if (!KeyDown)
    bitmask |= 0x04;

  KeyUsed = true;
  
  return bitmask;
}
///

/// Keyboard::ReadKeyCode
// Return the keyboard code of the last key pressed.
UBYTE Keyboard::ReadKeyCode(void)
{
  KeyUsed = true;
  
  return KeyCode;
}
///

/// Keyboard::KeyboardInterrupt
// Return true if a keyboard interrupt is pending, then clear
// the flag
bool Keyboard::KeyboardInterrupt(void)
{  
  KeyUsed = true;

  if (KeyIRQPending) {
    KeyIRQPending = false;
    return true;
  }
  return false;
}
///

/// Keyboard::BreakInterrupt
// Return true if a break interrupt is pending, then clear the
// flag
bool Keyboard::BreakInterrupt(void)
{
  if (BreakIRQPending) {
    BreakIRQPending = false;
    return true;
  }
  return false;
}
///

/// Keyboard::MakeKeyEvent
// Create or recycle a new key event.
struct Keyboard::KeyEvent *Keyboard::MakeKeyEvent(void)
{
  struct KeyEvent *ev;
  //
  // First try to recycle a free event.
  ev = FreeEvents.RemTail();
  if (ev == NULL) {
    // Need to build a new one now, really.
    ev = new KeyEvent;
  }
  //
  ev->Type = KeyEvent::Free;
  return ev;
}
///

/// Keyboard::HandleKey
// Press an ordinary key given by an ASCII code, shift flag will be modified
// from the code.
void Keyboard::HandleKey(bool press,char key,bool shift,bool control)
{
  struct KeyEvent *ev = MakeKeyEvent();
  //
  ev->Type        = KeyEvent::Regular;
  ev->Key         = key;
  ev->Shifted     = shift;
  ev->Control     = control;
  ev->KeyDown     = press;
  //
  EventList.AddHead(ev);
  if (!TypeAhead)
    FeedKey();
}
///

/// Keyboard::HandleSimpleKey
// Press an ordinary key given by an ASCII code, shift flag will be modified
// from the code.
void Keyboard::HandleSimpleKey(bool press,char key,bool shift,bool control)
{
  struct KeyEvent *ev = MakeKeyEvent();
  //
  ev->Type        = KeyEvent::Simple;
  ev->Key         = key;
  ev->Shifted     = shift;
  ev->Control     = control;
  ev->KeyDown     = press;
  //
  EventList.AddHead(ev);
  if (!TypeAhead)
    FeedKey();
}
///

/// Keyboard::FeedKey
// Remove the last keyboard event from the event list, forward to the internal
// state handling.
void Keyboard::FeedKey(void)
{  
  UBYTE keycode;
  UBYTE id;
  
  struct KeyEvent *ev = EventList.Last();
  if (ev) {
    // Keep the state of the keyboard.
    ShiftDown   = ev->Shifted;
    CtrlDown    = ev->Control;
    switch(ev->Type) {
    case KeyEvent::Regular:
    case KeyEvent::Simple:
      if (ev->KeyDown) {
	// Regular key goes down.
	Key         = ev->Key;
	MenuKeyDown = true;
	id          = UBYTE(ev->Key);  
	if (machine->MachType() == Mach_5200) {
	  keycode   = KeyCodes5200[id];
	  if (keycode != 0xff) {
	    // The key code is valid. Keep it and generate an IRQ
	    KeyCode       = keycode;
	    KeyDown       = true;
	    KeyIRQPending = true;
	  }
	  // Shift is always a valid modifier here, though independent on the key
	  if (ShiftDown)
	    keycode      |= 0x40;
	} else {
	  keycode   = KeyCodes[id];
	  if (keycode != 0xff) {
	    // The key code is valid. 
	    if (ev->Type == KeyEvent::Regular) {
	      // Get the ShiftDown flag from
	      // the Shift modification.
	      ShiftDown  = (ShiftCodes[id])?(true):(false);
	    }
	    if (ShiftDown)
	      keycode     |= 0x40;
	    if (CtrlDown)
	      keycode     |= 0x80;
	    //
	    KeyCode       = keycode;
	    KeyDown       = true;
	    KeyIRQPending = true;
	  }
	}
      } else {
	KeyDown       = false;
	MenuKeyDown   = false;
      }
      break;
    case KeyEvent::Special:
      if (ev->KeyDown) {
	keycode       = 0xff;
	Key           = 0x00;
	if (machine->MachType() == Mach_5200) {
	  // Here, some special keys generate generic keycodes nevertheless
	  switch(ev->Code) {
	  case Break:
	  case Shift:
	    // These two also generate a shift keyboard key.
	    ShiftDown        = true;
	    BreakIRQPending  = true;
	    KeyDown          = true;
	    break; 
	  case Start:
	    // This is keycode 0x0c. It does not go to GTIA but to POKEY
	    keycode          = 0x19;
	    break;
	  case Select:
	    // Um. This should be the "pause" key. Oh well, we re-use the
	    // keys a bit different on the 5200'er
	    keycode          = 0x11;
	    break;
	  case Option:
	    // This is the "reset" key
	    keycode          = 0x09;
	    break;
	  default:
	    break;
	  }
	} else {
	  switch(ev->Code) {
	  case Atari:
	    keycode          = 0x27;
	    break;
	  case Caps:      
	    keycode          = 0x3c;
	    break;
	  case Shift:
	    ShiftDown        = true;
	    break;
	  case Help:
	    keycode          = 0x11;
	    break;
	  case F1:
	    keycode          = 0x03;
	    break;
	  case F2:
	    keycode          = 0x04;
	    break;
	  case F3:
	    keycode          = 0x13;
	    break;
	  case F4:
	    keycode          = 0x14;
	    break;
	  default:
	    // Everything else should not be
	    // in this queue.
	    break;
	  }
	}
	//
	// Update the keycode now
	if (keycode != 0xff) {
	  if (ev->Shifted)
	    keycode       |= 0x40;
	  if (ev->Control)
	    keycode       |= 0x80;
	  //
	  KeyCode          = keycode;
	  KeyDown          = true;
	  KeyIRQPending    = true;
	}
      } else {
	// Special key up.
	if (machine->MachType() == Mach_5200) {
	  switch(ev->Code) {
	  case Break:
	  case Shift:
	    ShiftDown = false;
	    break;
	  case Start:
	  case Select:
	  case Option:
	    KeyDown  = false;
	    break;
	  default:
	    break;
	  }
	} else {
	  switch (ev->Code) {
	  case Shift:
	    ShiftDown = false;
	    break;
	  case Atari:
	  case Caps:
	  case Help:
	  case F1:
	  case F2:
	  case F3:
	  case F4:
	    KeyDown          = false;
	    break;
	  default:
	    break;
	  }
	}
      }
    case KeyEvent::Free:
      // Ignore this event.
      break;
    }
    //
    // Remove this event and recycle.
    ev->Remove();
    FreeEvents.AddHead(ev);
  }
}
///

/// Keyboard::HandleSpecial
// Press or release a special key.
void Keyboard::HandleSpecial(bool press,SpecialKey key,bool shift,bool control)
{
  struct KeyEvent *ev = MakeKeyEvent();

  ev->Shifted = shift;
  ev->Control = control;
  ev->Key     = 0x00;
  ev->Code    = key;
  ev->KeyDown = press;

  if (press) {
    if (machine->MachType() == Mach_5200) {
      // Here, some special keys generate generic keycodes nevertheless
      // We always have a key event here.
      switch(key) {
      case Break:
      case Shift:
	// These two also generate a shift keyboard key.
	BreakIRQPending  = true; // this happens synchroniously.
	// Runs into the following: Also generates a regular keyboard event.
      case Start:
      case Select:
      case Option:
	// These are all "regular" keys on the 5200 and must be
	// handled thru pokey.
	// This is the "reset" key
	ev->Type         = KeyEvent::Special;
	break;
      default:
	break;
      }
    } else {
      switch(key) {
      case Break:
	// Break key pressed
	BreakIRQPending  = true; // synchronously.
      break;
      case Atari:
      case Caps:      
      case Shift:
      case Help:
      case F1:
      case F2:
      case F3:
      case F4:
	ev->Type       = KeyEvent::Special; // Must handle these.
	break;
      case Option:
	ConsoleKeyFlags &= ~0x04;
	break;
      case Select:
	ConsoleKeyFlags &= ~0x02;
	break;
      case Start:
	ConsoleKeyFlags &= ~0x01;
	break;
      }
    }
  } else {
    if (machine->MachType() == Mach_5200) {
      switch(key) {
      case Break:
      case Shift:
      case Start:
      case Select:
      case Option:
	ev->Type    = KeyEvent::Special;
	break;
      default:
	break;
      }
    } else {
      switch (key) {
      case Break:
	break;
      case Shift:
      case Atari:
      case Caps:
      case Help:
      case F1:
      case F2:
      case F3:
      case F4:
	ev->Type      = KeyEvent::Special;
	break;
      case Option:
	ConsoleKeyFlags |= 0x04;
	break;
      case Select:
	ConsoleKeyFlags |= 0x02;
	break;
      case Start:
	ConsoleKeyFlags |= 0x01;
	break;
      }
    }
  }
  //
  if (ev->Type != KeyEvent::Free) {
    EventList.AddHead(ev);
    if (!TypeAhead)
      FeedKey();
  } else {
    FreeEvents.AddHead(ev);
  }
}
///

/// Keyboard::ReadKey
// For the menu frontend that gets feed by this keyboard class:
// read the key status (up/down) and the control/shift keys
bool Keyboard::ReadKey(char &key,bool &shift,bool &control)
{
  // Move the next event into the keyboard.
  if (KeyUsed) {
    FeedKey();
  }
  key     = Key;
  shift   = ShiftDown;
  control = CtrlDown;
  KeyUsed = true;
  return MenuKeyDown;
}
///

/// Keyboard::DisplayStatus
// Print the keyboard status over the monitor
void Keyboard::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Keyboard status:\n"
		   "\tHoldOption    : %s\tHoldSelect      : %s\tHoldStart : %s\n"
		   "\tKeyIRQPending : %s\tBreakIRQPending : %s\n"
		   "\tKeyDown       : %s\tShiftDown       : %s\n"
		   "\tKeyCode       : %02x\tConsoleKeyStatus: %02x\n",
		   (HoldOption)?("yes"):("no"),
		   (HoldSelect)?("yes"):("no"),
		   (HoldStart)?("yes"):("no"),
		   (KeyIRQPending)?("yes"):("no"),
		   (BreakIRQPending)?("yes"):("no"),
		   (KeyDown)?("yes"):("no"),
		   (ShiftDown)?("yes"):("no"),
		   KeyCode,ConsoleKeyFlags);
}
///

/// Keyboard::ParseArgs
// Parse off keyboard relevant arguments
void Keyboard::ParseArgs(class ArgParser *args)
{ 
  class KeyButtonController *ctrl;
  //
  args->DefineTitle("Keyboard");
  args->OpenSubItem("Keys"); // also into the title menu
  args->DefineBool("HoldOption","hold option on reset",HoldOption);
  args->DefineBool("HoldSelect","hold select on reset",HoldSelect);
  args->DefineBool("HoldStart","hold start on reset",HoldStart);
  args->DefineBool("BufferKeys","enable smart keyboard buffer",TypeAhead);
  args->CloseSubItem();
  //
  // Check whether we are in 5200 mode. If so, also allow to press buttons
  // as part of the keyboard.
  if (machine->MachType() == Mach_5200) {
    if (KeyButtonList.IsEmpty()) {
      // Build up the buttons.
      char keybuttonname[80];
      char target;
      //
      // Build up the key button events.
      for(target = '0';target <= '9';target++) {
	snprintf(keybuttonname,79,"KeyButton.%c",target);
	KeyButtonList.AddTail(new class KeyButtonController(machine,target,keybuttonname));
      }
      KeyButtonList.AddTail(new class KeyButtonController(machine,'#',"KeyButton.Hashmark"));
      KeyButtonList.AddTail(new class KeyButtonController(machine,'*',"KeyButton.Asterisk"));
    } 
    args->CloseSubItem();
    for(ctrl = KeyButtonList.First();ctrl;ctrl = ctrl->NextOf())
      ctrl->ParseArgs(args);
  }
}
///

