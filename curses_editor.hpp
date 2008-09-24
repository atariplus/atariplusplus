/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: curses_frontend.cpp,v 1.7 2008/05/25 18:17:30 thor Exp $
 **
 ** In this module: A full screen text editor using curses
 **********************************************************************************/

#ifndef CURSES_EDITOR_HPP
#define CURSES_EDITOR_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "curses.hpp"
#include "chip.hpp"
///

/// Forwards
class ArgParser;
class Monitor;
class Keyboard;
///

/// class Curses_Editor
#ifdef USE_CURSES
// This class emulates (as close as possible) a atari full screen
// editor and its special keys.
class Curses_Editor : public Chip {
  //
  // The curses window
  WINDOW      *window;
  //
  // Cleanup the curses interface.
  void ExitCurses(void);
  //
public:
  //
  // "Special" keyboard codes
  enum {
    Escape     = 0x1b,
    Up         = 0x1c,
    Down       = 0x1d,
    Left       = 0x1e,
    Right      = 0x1f,
    Clear      = 0x7d,
    Backspace  = 0x7e,
    Tab        = 0x7f,
    // The following mimic the A1200 special keys.
    Inverse    = 0x81, // The Atari key
    Caps       = 0x82,
    HiCaps     = 0x83, // shifted caps
    CtrlCaps   = 0x84, // ctrl+caps
    EOF        = 0x85, // ^3 = signal EOF
    Toggle     = 0x89, // toggle key-click on/off
    F1         = 0x8a,
    F2         = 0x8b,
    F3         = 0x8c,
    F4         = 0x8d,
    Home       = 0x8e,
    End        = 0x8f,
    LineLeft   = 0x90,
    LineRight  = 0x91,
    // Irregular (emulator-only) keys follow
    Break      = 0x92, // the break key
    Stop       = 0x93, // ^1: Holds the output.
    Help       = 0x94,
    SHelp      = 0x95, // shifted help
    // Regular A800 keys follow.
    EOL        = 0x9b,
    DeleteLine = 0x9c,
    InsertLine = 0x9d,
    DeleteTab  = 0x9e,
    InsertTab  = 0x9f,
    Bell       = 0xfd,
    DeleteChar = 0xfe,
    InsertChar = 0xff
  };
  //
public:
  Curses_Editor(class Machine *mach);
  virtual ~Curses_Editor(void);
  //
  // Start and restart the editor window.
  virtual void WarmStart(void);
  virtual void ColdStart(void);
  //
  // Parse the arguments off
  virtual void ParseArgs(class ArgParser *args);
  //
  // Print the status of the chip over the monitor
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Collect a keyboard key, return the extended
  // keyboard description similar to the internal
  // K: handler (extended codes). Returns -1 in
  // case no key is available. Does not busy-wait.
  int GetExtendedKey(void);
  //
  // Collect an un-extended keyboard key, as the K:
  // handler does. Returns -1 in case it is not available.
  int GetKey(void);
};
#endif
///

///
#endif
