/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_frontend.hpp,v 1.46 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: A simple X11 frontend without further GUI
 **********************************************************************************/

#ifndef X11_FRONTEND_HPP
#define X11_FRONTEND_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "machine.hpp"
#include "chip.hpp"
#include "xfront.hpp"
#include "keyboardstick.hpp"
#include "colorentry.hpp"
#include "screendump.hpp"
#include "string.hpp"
#ifndef X_DISPLAY_MISSING
#include <X11/Xlib.h>
///

/// Forwards
class ArgParser;
class Monitor;
class Keyboard;
class Timer;
///

/// Class X11_FrontEnd
// This class implements the simple X11 front-end
class X11_FrontEnd : public XFront {
  //
  // Link to the keyboard class we need to feed data into
  class Keyboard *keyboard;
  // Visual of the screen we reside in
  Visual         *visual;
  //
  // Depth of the screen in bit planes
  int             depth;
  //
  // The default colormap, and the (possibly created) private colormap
  Colormap        defcolormap;
  Colormap        custommap;  
  // The used color map
  Colormap        cmap;
  // Copy of the GTIA color map to get colormap changes
  const struct ColorEntry *colormap;
  //
  // Copy of the frame buffer variable.
  class X11_DisplayBuffer *framebuffer;
  //
  // Root window of the screen
  Window          root;
  //
  bool            isinit;        // set to true as soon as we have the display
  bool            ismapped;      // set to true as soon as the window is mapped (first exposure)
  bool            dump;          // set if screen dump is required/enabled
  bool            isgrabbed;     // set if the mouse is grabbed
  bool            havefocus;     // set if we have the input focus
  bool            truecolor;     // set if we would like a truecolor framebuffer
  bool            showcursor;    // set if the cursor is to be shown
  bool            button;        // state of the mouse button
  int             dumpcnt;       // counts the number of screen dumps
  int             scrolledlines; // counts the number of lines scrolled since the last query.
  // 
  // dimensions of the drawing area of the window
  int             emuwidth,emuheight;
  //
  // Dimensions of the frame buffer and other prefs
  LONG            LeftEdge,TopEdge;
  LONG            Width,Height;
  LONG            PixelWidth,PixelHeight;
  bool            PixMapIndirect;
  bool            EnableXVideo;
  //  
  // Atoms for communication with the window manager
  Atom            WMDeleteWindowAtom;
  Atom            WMProtocolsAtom;
  //
  // Cursor shapes for the mouse pointer
  Cursor          Shown;
  Cursor          Hidden;
  //
  //
  // Sub-classes for joystick input
  class KeyboardStick   *KeypadStick;
  //
  class MouseMoveStick : public GamePort {
    //
    // Previous position. Used to detect motion in the relative mode.
    int  lastx,lasty;
    //
    // Last time the position was checked
    int  lastsec,lastusec;
    //
    // Last movement, to avoid erratic non-movements.
    int  lastdx,lastdy;
    //
    // Absolute or relative mouse movements?
    bool isrel;
    //
  public:
    // Constructor: This class is called MouseStick
    MouseMoveStick(class Machine *mach,const char *name,bool relative)
      : GamePort(mach,name,0), lastx(0), lasty(0),
	lastsec(0), lastusec(0),
	lastdx(0), lastdy(0), isrel(relative)
    { }
    //
    ~MouseMoveStick(void)
    { }
    //
    // Just transmit data to the input chain here. Returns true if
    // we need to confine the mouse to the window.
    bool TransmitStates(Display *d,Window win,int width,int height,bool paused);
    //
  }               MouseStick,RelMouseStick;
  //
  //
  // X11 FrontEnd preferences start here
  bool            PrivateCMap; // set to true if we should try to allocate a private cmap
  bool            SyncX;       // set to true if we want an XSync
  bool            DisableDPMS; // set to true if we want to disable screen blanking
  //
  // Default base name for the screen dumps
  char           *PictureBaseName;
  //
  // Format of the screen dump
  ScreenDump::GfxFormat Format;
  //
  //
  // Implementation of the VBI activity required to drive the display frontend
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  // Build up the connection to the X server and open all the displays etc...
  void CreateDisplay(void);
  // Close the current display
  void CloseDisplay(void);
  //
  // Handle all events currently on the X event queue
  void HandleEventQueue(void);
  //
  // On a keyboard event, forward the corresponding data to the keyboard
  // class
  void HandleKeyEvent(XKeyEvent *event);
  // Handle a window manager client message
  void HandleClientMessage(XClientMessageEvent *event);
  // Handle focus in and focus out
  void HandleFocusChange(XFocusChangeEvent *event);
  // Confine the mouse into our window if this is allowed.
  void GrabMouse(bool grabme);
  // Handle a mouse button event.
  void HandleButtonEvent(XButtonEvent *event);
  //
  // Make a screen dump of the currently active frame
  void DumpScreen(void);
  //
  // Create the frame buffer in the right mode.
  class X11_DisplayBuffer *GetFrameBuffer(void);
  //
  // Override the base unload framebuffer to reset our state.
  void UnloadFrameBuffer(void);
  //
public:
  X11_FrontEnd(class Machine *mach,int unit);
  virtual ~X11_FrontEnd(void);
  //
  //
  // Coldstart and warmstart methods for the chip class.
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Argument parser frontends
  virtual void ParseArgs(class ArgParser *args);
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Return the active buffer we must render into
  virtual UBYTE *ActiveBuffer(void);  
  //
  // Signal that we start the display generation again from top. Hence, this implements
  // some kind of "vertical sync" signal for the display generation.
  virtual void ResetVertical(void);
  //
  // If we want to emulate a flickering drive LED, do it here.
  virtual void SetLED(bool on);  
  //
  // Enforce a full display refresh, i.e. after the monitor returnes
  virtual void EnforceFullRefresh(void);
  //
  // Enforce the screen to foreground or background to run the monitor
  virtual void SwitchScreen(bool foreground);  
  //  
  // Enable or disable the mouse pointer
  virtual void ShowPointer(bool showit);
  //
  // For GUI frontends within this buffer: Get the position and the status
  // of the mouse within this buffer measured in internal coordinates.
  virtual void MousePosition(LONG &x,LONG &y,bool &button);
  //
  // Set the mouse position to the indicated coordinate
  virtual void SetMousePosition(LONG x,LONG y);
  //
  // Return the number of lines scrolled since we asked last.
  virtual int  ScrollDistance(void);
  //
  // For GUI frontends: Check whether the mouse is available as input
  // device. This returns false in case the mouse is an input device
  // and has been grabbed on its "GamePort".
  virtual bool MouseIsAvailable(void);
  //
  // Disable or enable double buffering temporarely for the user
  // menu. Default is to have it enabled.
  virtual void EnableDoubleBuffer(bool enable);
};
///

///
#endif // of if HAVE_LIBX11
#endif
///
