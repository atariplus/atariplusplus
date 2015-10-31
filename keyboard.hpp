/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: keyboard.hpp,v 1.17 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Definition of the interface towards keyboard input
 **********************************************************************************/

#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

/// Includes
#include "types.hpp"
#include "chip.hpp"
#include "vbiaction.hpp"
#include "gamecontrollernode.hpp"
///

/// Forwards
class Machine;
class ArgParser;
class Monitor;
class Timer;
///

/// Class Keyboard
// This keyboard abstracts from the Atari keyboard mapping
class Keyboard : public Chip, private VBIAction {
public:
  // Special keys that cannot be encoded as ASCII
  enum SpecialKey {
    Break,                    // the break key
    Atari,                    // the atari = inverse video key
    Caps,                     // the Caps key
    Shift,                    // the Shift key alone
    Help,                     // the help key
    F1,                       // the F1 key
    F2,                       // the F2 key
    F3,                       // the F3 key
    F4,                       // the F4 key
    Option,                   // the option key
    Select,                   // the select key
    Start                     // the start key
  };
private:
  //
  // This enum describes how the shift-selection shall be changed for a given
  // ascii key.
  // The keyboard mapping table without shift
  static const UBYTE KeyCodes[128];             // keycodes without shift
  static const UBYTE ShiftCodes[128];           // how to interpret shift
  static const UBYTE KeyCodes5200[128];         // keycodes on the 5200
  //
  // Some private variables
  //
  bool    KeyIRQPending;   // keyboard interrupt still pending?
  bool    BreakIRQPending; // break interrupt still pending?
  bool    ResetPending;    // reset interrupt pending?
  //
  //
  bool    KeyDown;         // the last key is still held down
  bool    MenuKeyDown;     // for the menu frontend: Also supplies keys the Atari doesn't have
  bool    ShiftDown;       // the shift key is held down
  bool    CtrlDown;        // the control key is held down
  //
  //
  char    Key;             // currently active key in ASCII
  UBYTE   KeyCode;         // currently active keycode
  UBYTE   ConsoleKeyFlags; // console key mask
  //
  // The following boolean gets set in case the last pressed
  // key has been read (or we guess it has been read) by the
  // emulated machine. Provide a new one if there's one in the
  // input queue.
  bool    KeyUsed;
  //
  //
  int     ResetCount;      // Counts how LONG OPTION should be held.
  //  
  // And now for the keyboard event buffer.
  struct KeyEvent : public Node<struct KeyEvent> {
    //
    // First the type of key we're encoding here.
    enum {
      Free,                  // unused event, recycle.
      Simple,                // vanilla keypress using SHIFT from the keyboard
      Regular,               // keypress with special SHIFT interpretation
      Special                // Special keypress, BRK and similar stuff.
    }           Type;
    char        Key;         // Regular key code
    SpecialKey  Code;        // used only for special keyboard functions
    bool        Shifted;     // true if shift is pressed aLONG with the key
    bool        Control;     // true if Ctrl is pressed aLONG with the key
    bool        KeyDown;     // true if pressed, false if a keyboard release event.
  };
  //
  // A gamecontroller input class that is used to emulate button presses
  // to various 5200 "keypad" buttons.
  class KeyButtonController : public GameControllerNode, public Node<KeyButtonController> {
    //
    // Enable/disable switch of this controller.
    bool Enabled;
    //
    // The character we map this button to.
    char Target;
    //
    // The button we listen to.
    LONG Button;
    //
  public:
    KeyButtonController(class Machine *mach,char keycode,const char *name);
    ~KeyButtonController(void);
    //
    // Implementation of the configurable interface
    virtual void ParseArgs(class ArgParser *args);
    //
    // Feed input events into the class and thus into the keyboard.
    // We only need to overload the button handling.
    virtual void FeedButton(bool value,int button = 0);
    //
    //
    // The game controller has a next and prev, but as it is part of
    // two lists, we need to ensure that we get the right one. Of course
    // we mean the next of the game controller and not the next of
    // the configchain.
    class KeyButtonController *NextOf(void) const
    {
      return Node<KeyButtonController>::NextOf();
    }
    class KeyButtonController *PrevOf(void) const
    {
      return Node<KeyButtonController>::PrevOf();
    }
  };
  //
  // The list of keyboard events.
  List<KeyEvent>            EventList;
  List<KeyEvent>            FreeEvents;
  //
  // The list of key buttons in case we have any.
  List<KeyButtonController> KeyButtonList;
  //
  // Preferences
  bool                      HoldOption;      // hold option on reset
  bool                      HoldSelect;      // hold select on reset
  bool                      HoldStart;       // hold start on reset
  bool                      TypeAhead;       // enable type ahead buffer
  //
  // Create or recycle a new key event.
  struct KeyEvent *MakeKeyEvent(void);
  // Remove the last keyboard event from the event list, forward to the internal
  // state handling.
  void FeedKey(void);
  //
  // Notification from the timer that a new VBI is active. This "eats" possibly
  // read keys.
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
public:
  // Constructor and destructor
  Keyboard(class Machine *machine);
  ~Keyboard(void);
  //
  // Input methods for the emulator front-end: Use these to feed data into
  // the keyboard.
  //
  // Press an ordinary key given by an ASCII code, shift flag will be modified
  // from the code.
  void HandleKey(bool press,char key,bool shift,bool control);
  //
  // The following is similar to the above, except that the shift flag is
  // directly taken from the flag and not from the ascii value
  void HandleSimpleKey(bool press,char key,bool shift,bool control);
  //
  // Press a special key given by its id
  void HandleSpecial(bool press,SpecialKey key,bool shift,bool control);
  //
  // Interface methods for the emulator: These are read by Pokey and GTIA
  // 
  // Read the status of all console keys excluding HELP
  UBYTE ConsoleKeys(void);
  //
  // Read the keyboard status flags for Pokey SKSTAT
  UBYTE KeyboardStatus(void);
  //
  // Read the keyboard code including Control and Shift bits
  UBYTE ReadKeyCode(void);
  //
  // For the menu frontend that gets feed by this keyboard class:
  // read the key status (up/down) and the control/shift keys
  bool ReadKey(char &key,bool &shift,bool &control);
  //
  // Return true if a keyboard interrupt is pending, then clear
  // the flag
  bool KeyboardInterrupt(void);
  //
  // Return true if a break interrupt is pending, then clear the
  // flag
  bool BreakInterrupt(void);
  //
  // Return true if Reset has been pressed
  bool ResetHeld(void);
  //
  //
  // Methods imported from the chip class
  //
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  //
  virtual void ParseArgs(class ArgParser *arg);
  virtual void DisplayStatus(class Monitor *mon);
};
///

///
#endif
