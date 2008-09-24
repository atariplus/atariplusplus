/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: curses_frontend.hpp,v 1.6 2008/05/25 18:17:30 thor Exp $
 **
 ** In this module: A frontend using the curses library for text output
 **********************************************************************************/

#ifndef CURSES_FRONTEND_HPP
#define CURSES_FRONTEND_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "curses.hpp"
#include "display.hpp"
#include "keyboardstick.hpp"
///

/// Forwards
class ArgParser;
class Monitor;
class Keyboard;
class Timer;
class Antic;
class MMU;
///

/// class Curses_FrontEnd
#ifdef USE_CURSES
class Curses_FrontEnd : public AtariDisplay {
  //
  // The characters on the screen, last generation.
  char        *LastDisplayBuffer;
  //
  // The characters on the screen, this generation.
  char        *DisplayBuffer;
  //
  // Input buffer for GTIA output, even though unused.
  UBYTE       *InputBuffer;
  //
  // The curses window
  WINDOW      *window;
  //
  // Dimensions of the buffer.
  enum {
    BufferWidth  = 48,
    BufferHeight = 32
  };
  //
  // The antic, we collect data from here.
  class Antic    *Antic;
  //
  // The memory management, to gain access to
  // the display buffer.
  class MMU      *MMU;
  //
  class Keyboard *keyboard;
  //
  // Set in case a full refresh is required.
  bool            fullrefresh;
  //
  // Set in case the caps-lock key must be pressed
  // to make sure we get lower-case characters.
  bool            sendcaps;
  //
  // Counter when start, option and select are raised again
  // after being pressed. This is emulated as curses only
  // reacts on the key-down event.
  int             optioncnt,selectcnt,startcnt;
  // 
  // Sub-classes for joystick input: This remains open.
  class KeyboardStick KeypadStick;
  //
  // Analyze the display and copy it into the current
  // display buffer.
  void AnalyzeDisplay(void);
  //
  // Start the curses front-end/display
  void InitCurses(void);
  //
  // Disable the curses front-end/display
  void ExitCurses(void);
  //
  // Draw the contents of the screen buffer to the curses output
  void RedrawScreen(void);
  //
  // Handle keyboard input - the only type of event
  // ncurses can create.
  void HandleEventQueue(void);
  //
  // The vertical blank method: Updates the screen.
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
public:
  Curses_FrontEnd(class Machine *mach);
  virtual ~Curses_FrontEnd(void);
  //
  // Coldstart and warmstart methods for the chip class.
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Return the active buffer we must render into. This
  // is actually unused.
  virtual UBYTE *ActiveBuffer(void)
  {
    return NULL;
  }
  // 
  // Return the next output buffer for the next scanline to generate.
  // This either returns a temporary buffer to place the data in, or
  // a row in the final output buffer, depending on how the display generation
  // process works.
  virtual UBYTE *NextScanLine(void)
  {
    return InputBuffer;
  }
  //
  // Push the output buffer back into the display process. This signals that the
  // currently generated display line is ready.
  virtual void PushLine(UBYTE *,int)
  {
  }  
  //
  // Signal that we start the display generation again from top. Hence, this implements
  // some kind of "vertical sync" signal for the display generation.
  virtual void ResetVertical(void)
  {
  }
  //
  // If we want to emulate a flickering drive LED, do it here.
  virtual void SetLED(bool)
  {
  }
  //
  // Enforce a full display refresh, i.e. after the monitor returnes
  virtual void EnforceFullRefresh(void)
  {
    fullrefresh = true;
  }
  //
  // Enforce the screen to foreground or background to run the monitor
  virtual void SwitchScreen(bool foreground);
  //
  // Enable or disable the mouse pointer
  virtual void ShowPointer(bool)
  {
  }
  //
  // Get the layout of the buffer
  virtual void BufferDimensions(LONG &leftedge,LONG &topedge,
				LONG &width,LONG &height,LONG &modulo)
  {
    leftedge = 0;
    topedge  = 0;
    width    = 0;
    height   = 0;
    modulo   = 0;
  }
  //
  // For GUI frontends within this buffer: Get the position and the status
  // of the mouse within this buffer measured in internal coordinates.
  virtual void MousePosition(LONG &x,LONG &y,bool &button)
  {
    x = 0,y = 0;
    button = false;
  }
  //
  // Set the mouse position to the indicated coordinate
  virtual void SetMousePosition(LONG,LONG)
  {
  }
  //
  // Return the current scroll distance generated by a mouse wheel.
  virtual int ScrollDistance()
  {
    return 0;
  }
  //
  // For GUI frontends: Check whether the mouse is available as input
  // device. This returns false in case the mouse is an input device
  // and has been grabbed on its "GamePort".
  virtual bool MouseIsAvailable(void)
  {
    return false;
  }
  //
  // Signal the requirement for refresh in the indicated region. This is only
  // used by the GUI engine as the emulator core uses PushLine for that.
  virtual void SignalRect(LONG,LONG,LONG,LONG)
  {
  }
  //
  // Disable or enable double buffering temporarely for the user
  // menu. Default is to have it enabled.
  virtual void EnableDoubleBuffer(bool)
  {
  }
  //
  // Test whether the user requests the menu by pointing the mouse into
  // the menu bar and holding the button. Returns true if so.
  bool MenuVerify(void)
  {
    return false;
  }
  //
  // Parse off command line arguments.
  virtual void ParseArgs(class ArgParser *args);
  //
  // Print the status.
  virtual void DisplayStatus(class Monitor *mon);
};
#endif
///

///
#endif