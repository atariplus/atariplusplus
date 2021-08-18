/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_frontend.cpp,v 1.89 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: A simple X11 frontend without further GUI
 **********************************************************************************/

/// Includes
#include "types.h"
#include "xfront.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "x11_frontend.hpp"
#include "keyboard.hpp"
#include "gamecontroller.hpp"
#include "exceptions.hpp"
#include "x11_displaybuffer.hpp"
#include "x11_xvideobuffer.hpp"
#include "antic.hpp"
#include "gtia.hpp"
#include "stdio.hpp"
#include "new.hpp"
#ifndef X_DISPLAY_MISSING
// Enforce inclusion of X keymap defiitions
#define XK_MISCELLANY
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dpms.hpp"
#define XK_XKB_KEYS
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>
#include "time.hpp"
#ifndef HAS_STRUCT_TIMEVAL
struct timeval {
  int tv_sec;       /* Seconds.  */
  int tv_usec;      /* Microseconds.  */
};
#endif
///

/// ErrorHandler
// This is the X11 error handler. It is unclear why
// some X events I run here fail - I haven't been able
// to find *any* reason why re-building the window fails
// from time to time. Anyhow, it does. And as the error
// handler gets no user-defined data, we cannot even make
// this a member function. Just close the display, and
// reopen it again seems to be enough to fix it.
static bool fixerror = false;
static XErrorEvent *last_xev = NULL;

static int ErrorHandler(Display *, XErrorEvent *ev)
{
  last_xev = ev;
  fixerror = true;
  return 0;
}
///

/// X11_FrontEnd::X11_FrontEnd
X11_FrontEnd::X11_FrontEnd(class Machine *mach,int unit)
  : XFront(mach,unit), visual(NULL), depth(0), defcolormap(0), custommap(0), 
    colormap(NULL), framebuffer(NULL), root(0),
    isinit(false), ismapped(false), dump(false), grab(false), isgrabbed(false), havefocus(false), 
    truecolor(false), showcursor(true), button(false), dumpcnt(1), scrolledlines(0),
    LeftEdge(unit?0:16), TopEdge(0), 
    Width(unit?80*8:Antic::WindowWidth), Height(unit?25*8:Antic::WindowHeight),
    PixelWidth(2), PixelHeight(2), PixMapIndirect(false), EnableXVideo(false),
    WMDeleteWindowAtom(0), WMProtocolsAtom(0), Shown(0), Hidden(0),
    KeypadStick(NULL), MouseStick(mach,"MouseStick",false), RelMouseStick(mach,"RelMouseStick",true), 
    PrivateCMap(false), SyncX(false), DisableDPMS(true),
    PictureBaseName(new char[strlen("ScreenDump") + 1]), Format(ScreenDump::PNM)
{  
  if (Unit == 0) {
    strcpy(PictureBaseName,"ScreenDump");
  } else {
    strcpy(PictureBaseName,"XEPDump");
  }
}
///

/// X11_FrontEnd::~X11_FrontEnd
X11_FrontEnd::~X11_FrontEnd(void)
{
  //
  // Clean up the base name for the image
  delete[] PictureBaseName;
  // Clean the mess up behind me
  //
  CloseDisplay();    
}
///

/// X11_FrontEnd::GetFrameBuffer
// Create the frame buffer in the right mode.
class X11_DisplayBuffer *X11_FrontEnd::GetFrameBuffer(void)
{
  if (framebuffer == NULL && isinit) { 
    truecolor   = machine->GTIA()->SuggestTrueColor();
    framebuffer = XFront::FrameBufferOf(truecolor && depth > 8,EnableXVideo);
    try {
      framebuffer->ConnectToX(display,screen,window,cmap,
			      LeftEdge,TopEdge,Width,Height,
			      PixelWidth,PixelHeight,PixMapIndirect);
    } catch(AtariException &aex) {
      if (EnableXVideo) {
	XFront::UnloadFrameBuffer();
	framebuffer  = NULL;
	EnableXVideo = false;
	fixerror     = false;
	framebuffer  = XFront::FrameBufferOf(truecolor && depth > 8,false);
	framebuffer->ConnectToX(display,screen,window,cmap,
				LeftEdge,TopEdge,Width,Height,
				PixelWidth,PixelHeight,PixMapIndirect);
      } else {
	UnloadFrameBuffer();
	throw;
      }
    }
  }
  return framebuffer;
}
///

/// X11_FrontEnd::UnloadFrameBuffer
// Override the base unload framebuffer to reset our state.
void X11_FrontEnd::UnloadFrameBuffer(void)
{
  XFront::UnloadFrameBuffer();
  framebuffer = NULL;
}
///

/// X11_FrontEnd::CloseDisplay
// Cleanup the X11 FrontEnd display
void X11_FrontEnd::CloseDisplay(void)
{
  if (isinit) {
    //
    // Make the mouse available again
    GrabMouse(false);
    // Re-enable auto repeat
    XAutoRepeatOn(display);
#ifdef USE_DPMS
    enableDPMS(display);
#endif
    // Detach the display buffer from the X server
    if (framebuffer)
      framebuffer->DetachFromX();
    //
    // Dispose cursor shapes
    if (Shown) {
      XFreeCursor(display,Shown);
      Shown = 0;
    }
    if (Hidden) {
      XFreeCursor(display,Hidden);
      Hidden = 0;
    }
    // Check whether we have a custom color map. If so,
    // un-install it and install the previously active cmap now
    if (custommap) {
      XUninstallColormap(display,custommap);
      // And release it
      XFreeColormap(display,custommap);
      custommap = 0;
    } 
    // Sync and discard all events on the event queue
    XSync(display,true);
    // Remove the window from the screen now.
    if (window) {
      ismapped = false;
      XUnmapWindow(display, window);
      XDestroyWindow(display, window);
      window   = 0;
    }
    // 
    // Sync and discard all events on the event queue
    XSync(display,true);
    //
    // And unlink us from the X server
    XCloseDisplay(display);
    display = 0;
  }
  isinit  = false;
}
///

/// X11_FrontEnd::CreateDisplay
// Open the (simple) X11 display
void X11_FrontEnd::CreateDisplay(void)
{
  static const char data[2] = "\x01";
  XSetWindowAttributes xswda;
  XSizeHints           xhints;   
  XColor fg;
  Pixmap cursor = None;
  //
  // Get the keyboard from the machine.
  keyboard    = machine->Keyboard();
  KeypadStick = machine->KeypadStick();
  //
  // Associate the keyboard codes.
  KeypadStick->AssociateKey(KeyboardStick::ArrowLeft    ,XK_Left);
  KeypadStick->AssociateKey(KeyboardStick::ArrowRight   ,XK_Right);
  KeypadStick->AssociateKey(KeyboardStick::ArrowUp      ,XK_Up);
  KeypadStick->AssociateKey(KeyboardStick::ArrowDown    ,XK_Down);
  KeypadStick->AssociateKey(KeyboardStick::Return       ,XK_Linefeed,XK_Return);
#ifdef XK_ISO_Left_Tab
  KeypadStick->AssociateKey(KeyboardStick::Tab          ,XK_Tab,XK_ISO_Left_Tab);
#else
  KeypadStick->AssociateKey(KeyboardStick::Tab          ,XK_Tab);
#endif
  KeypadStick->AssociateKey(KeyboardStick::Backspace    ,XK_BackSpace);
  KeypadStick->AssociateKey(KeyboardStick::KP_0         ,XK_KP_Insert   ,XK_KP_0);
  KeypadStick->AssociateKey(KeyboardStick::KP_1         ,XK_KP_End      ,XK_KP_1);
  KeypadStick->AssociateKey(KeyboardStick::KP_2         ,XK_KP_Down     ,XK_KP_2);
  KeypadStick->AssociateKey(KeyboardStick::KP_3         ,XK_KP_Page_Down,XK_KP_3);
  KeypadStick->AssociateKey(KeyboardStick::KP_4         ,XK_KP_Left     ,XK_KP_4);
  KeypadStick->AssociateKey(KeyboardStick::KP_5         ,XK_KP_Begin    ,XK_KP_5);
  KeypadStick->AssociateKey(KeyboardStick::KP_6         ,XK_KP_Right    ,XK_KP_6);
  KeypadStick->AssociateKey(KeyboardStick::KP_7         ,XK_KP_Home     ,XK_KP_7);
  KeypadStick->AssociateKey(KeyboardStick::KP_8         ,XK_KP_Up       ,XK_KP_8);
  KeypadStick->AssociateKey(KeyboardStick::KP_9         ,XK_KP_Page_Up  ,XK_KP_9);
  KeypadStick->AssociateKey(KeyboardStick::KP_Divide    ,XK_KP_Divide);
  KeypadStick->AssociateKey(KeyboardStick::KP_Times     ,XK_KP_Multiply);
  KeypadStick->AssociateKey(KeyboardStick::KP_Minus     ,XK_KP_Subtract);
  KeypadStick->AssociateKey(KeyboardStick::KP_Plus      ,XK_KP_Add);
  KeypadStick->AssociateKey(KeyboardStick::KP_Enter     ,XK_KP_Enter);
  KeypadStick->AssociateKey(KeyboardStick::KP_Digit     ,XK_KP_Separator);
  KeypadStick->AssociateKey(KeyboardStick::SP_Insert    ,XK_Insert);
  KeypadStick->AssociateKey(KeyboardStick::SP_Delete    ,XK_Delete);
  KeypadStick->AssociateKey(KeyboardStick::SP_Home      ,XK_Home,XK_Home,XK_Begin);
  KeypadStick->AssociateKey(KeyboardStick::SP_End       ,XK_Home,XK_End);
  KeypadStick->AssociateKey(KeyboardStick::SP_ScrollUp  ,XK_Page_Up,XK_Prior);
  KeypadStick->AssociateKey(KeyboardStick::SP_ScrollDown,XK_Page_Down,XK_Next);
  //
  // Compute the size of the required display
  emuwidth  = Width  * PixelWidth;
  emuheight = Height * PixelHeight;
  //
  // Open the connection to the X server
  //
  display = XOpenDisplay(NULL);
  if (display == NULL) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Cannot connect to the X server.");
  } 
  // Sync and discard all events on the event queue
  XSync(display,true);
  fixerror = false;
  XSetErrorHandler(&ErrorHandler);
  //
  // Set the init flag now, we have something to shut down.
  isinit  = true;
  //
  screen = DefaultScreenOfDisplay(display);
  if (screen == NULL) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Cannot get the default screen of the X server.");
  }
  
  visual = XDefaultVisualOfScreen(screen);
  if (visual == NULL) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Cannot get the visual of the screen.");
  }

  root   = RootWindowOfScreen(screen);
  if (root == 0) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Cannot get the root window of the screen.");
  }
  //
  // get the depth of the screen
  depth  = DefaultDepthOfScreen (screen);
  //
  // and the default default color map
  defcolormap = XDefaultColormapOfScreen(screen);
  custommap   = 0;

  if (PrivateCMap) {
    custommap = XCreateColormap(display,root,visual,AllocNone);
    cmap      = custommap;
  } else {
    cmap      = defcolormap;
  }

  //
  // Now create the root window of this application
  //
  memset(&xswda,0,sizeof(xswda));
  xswda.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask | FocusChangeMask | 
    ButtonPress | ButtonRelease;
  xswda.colormap   = cmap;
  //
  window = XCreateWindow (display,root,0,0,emuwidth,emuheight,3,depth,
			  InputOutput, visual,
			  CWEventMask | CWBackPixel | CWColormap,
			  &xswda);
  if (window == 0) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Failed to create the main emulator window");
  }
  //
  // Allocate cursor shapes
  Shown  = XCreateFontCursor(display,XC_arrow);
  if (Shown == None) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Failed to create the active cursor");
  }
  // Create a cursor shape of a single dot in the middle.
  cursor = XCreateBitmapFromData(display,window,data,1,1);
  fg.red = fg.green = fg.blue = 0;
  if (cursor) {
    Hidden = XCreatePixmapCursor(display,cursor,cursor,&fg,&fg,0,0);
    XFreePixmap(display,cursor);
  } else {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Failed to create the blank cursor shape");
  }
  if (Hidden == None) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Failed to create the blank cursor");
  }
  //
  // Set the window title
  XStoreName (display, window, machine->WindowTitle());
  //
  // Setup the hints for the window manager
  memset(&xhints,0,sizeof(xhints));
  xhints.flags = PSize | PMinSize | PMaxSize;
  xhints.x     = 0;
  xhints.y     = 0;
  xhints.min_width = xhints.max_width = xhints.width = emuwidth;
  xhints.min_height= xhints.max_height= xhints.height= emuheight;

  XSetWMProperties(display,window,
		   NULL, // "Atari++", // window name FIXME: Must be XTextProperty
		   NULL, // "Atari++", // icon name
		   NULL,      // argv,
		   0,         // argc,
		   &xhints,   // "normal hints"
		   NULL,      // XWMHints: Icon pixmap and similar
		   NULL       // XClassHint
		   );
  //
  // Now connect to the display buffer: Force a rebuild.
  UnloadFrameBuffer();    
  truecolor       = machine->GTIA()->SuggestTrueColor();
  colormap        = machine->GTIA()->ActiveColorMap();
  //
  // Now create the window manager atoms for the window close modification
  WMDeleteWindowAtom = XInternAtom(display, "WM_DELETE_WINDOW", false);
  if (WMDeleteWindowAtom == None) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Failed to create the window delete atom");
  }
  WMProtocolsAtom    = XInternAtom(display, "WM_PROTOCOLS", false);
  if (WMProtocolsAtom == None) {
    Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	  "Failed to create the window protocol atom");
  }
  //
  // Modify the window Protocol to get notified on a window close
  XSetWMProtocols(display, window, &WMDeleteWindowAtom,1);
  //
  // Now render the window
  XMapWindow(display, window);		
  XAutoRepeatOff(display);
  XDefineCursor(display,window,(showcursor)?(Shown):(Hidden));
#ifdef USE_DPMS
  disableDPMS(display,DisableDPMS);
#endif
  // This will build a new one. Must happen here in case the frame buffer
  // setup throws.
  try {
    GetFrameBuffer();
  } catch(AtariException &aex) {
    UnloadFrameBuffer();
    throw aex;
  }
  //
  XSync(display,false);    
  //XSetInputFocus(display,window,RevertToNone,CurrentTime);
  if (truecolor && depth <= 8) {
    machine->PutWarning("Advanced true color processing bypassed since "
			"no true color display is available.");
  }
  //
  // If this is an xvideo frame buffer and we failed, try again. Otherwise,
  // fail really!
  if (fixerror) {
    if (EnableXVideo) {
      UnloadFrameBuffer();
      EnableXVideo = false; 
      // Sync and discard all events on the event queue
      XSync(display,true);
      fixerror     = false;
      //
      machine->PutWarning("XVideo display not available.");
    } else {
      UnloadFrameBuffer();
      Throw(ObjectDoesntExist,"X11_FrontEnd::CreateDisplay",
	    "Unable to create an X11 display.\n"
	    "Sorry, no graphical output available.\n");
    }
  }
}
///

/// X11_FrontEnd::HandleEventQueue
// get all events from the event queue and run the
// apropriate actions
void X11_FrontEnd::HandleEventQueue(void)
{
  //
  // Check whether we run an XSync. You only want to do this
  // on a local machine as it otherwise slows down the connection
  // by requiring waiting for the X server to finish.
  // It *may* help performance locally a bit.
  //
  if (SyncX) {
    XSync(display,false); // do not discard the event queue
  }
  //
  // Now clean up the event queue by handling one event after
  // another
  while(XEventsQueued(display,QueuedAfterFlush /*QueuedAlready*/) > 0) {
    XEvent event; 
    // Get the next event. Since we checked above, we will not block
    // here.
    XNextEvent(display,&event);
    switch(event.type) {
    case ButtonPress:
    case ButtonRelease:
      // A button press/release event.
      HandleButtonEvent((XButtonEvent *)&event);
      break;
    case Expose:
      // An exposure event. Let the buffer now.
      ismapped = true;
      GetFrameBuffer()->HandleExposure();
      break;
    case KeyPress:    
    case KeyRelease:
      // A keyboard press event. Handle it in the keyboard handler
      HandleKeyEvent((XKeyEvent *)&event);
      break;
    case ClientMessage:
      // Messages from the window manager
      HandleClientMessage((XClientMessageEvent *)&event);
      break;
    case FocusIn:
    case FocusOut:
      // Focus change events
      HandleFocusChange((XFocusChangeEvent *)&event);
      break;
      // More events might follow later
    }
  }
}
///

/// X11_FrontEnd::HandleFocusChange
// Handle focus in and focus out
void X11_FrontEnd::HandleFocusChange(XFocusChangeEvent *event)
{
  // We currently toggle auto repeat here.
  if (event->type == FocusIn) {
    havefocus = true;
    XAutoRepeatOff(display);
  } else {    
    havefocus = false;
    XAutoRepeatOn(display);
  }
}
///

/// X11_FrontEnd::HandleClientMessage
// Handle a window manager client message
void X11_FrontEnd::HandleClientMessage(XClientMessageEvent *event)
{
  if ((event->message_type ==       WMProtocolsAtom) &&
      (event->data.l[0]    == (long)WMDeleteWindowAtom)) {
    // Shut down the window 
    machine->Quit() = true;
  }
  //
}
///

/// X11_FrontEnd::HandleKeyEvent
// On a keyboard event, forward the corresponding data to the keyboard
// class
void X11_FrontEnd::HandleKeyEvent(XKeyEvent *event)
{
  KeySym keysym;
  bool downflag;
  bool shift,control;

  downflag = (event->type==KeyPress);
  // If we don't have the focus, do not react on keydown events
  // This doesn't work reliable for twm, so disable.
  /*
  if (downflag && havefocus == false)
    return;
  */

  shift    = ((event->state) & ShiftMask)  ?(true):(false);
  control  = ((event->state) & ControlMask)?(true):(false);
  // Check whether we have to deal with a simple or a complicated keyboard
  // event. Keyboard events with only shift and control pressed can be
  // handled directly. Everything else will be processed thru the
  // keyboard mapping.
  if (event->state & ((1<<13) | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)) {
    char buffer[128];
    int numkeys;
    //
    // Now lookup the string.
    numkeys = XLookupString(event,buffer,sizeof(buffer),&keysym,NULL);
    if (numkeys == 0) {
      keysym   = XLookupKeysym(event,shift);
    } else if (numkeys != 1)
      return;
  } else {
    keysym   = XLookupKeysym(event,shift);
  }
  //
  // Check whether the keypad stick uses this key. If so, perform a mouse
  // movement.
  if (KeypadStick && showcursor == false) {
    if (KeypadStick->HandleJoystickKeys(downflag,keysym)) {
      return;
    }
  }
  // For convenience reasons, keysyms in range 0x00..0x7f are simply
  // ASCII codes
  if (keysym < 0x80) {
    keyboard->HandleKey(downflag,char(keysym & 0x7f),shift,control);
  } else switch(keysym) {
  case XK_Shift_L:
  case XK_Shift_R:
    keyboard->HandleSpecial(downflag,Keyboard::Shift,shift,control);
    break;
  case XK_Caps_Lock:
  case XK_Shift_Lock:
    keyboard->HandleSpecial(downflag,Keyboard::Caps,shift,control);
    break;
  case XK_Alt_L:
    //case XK_Alt_R:
  case XK_Super_L:
  case XK_Super_R:
    keyboard->HandleSpecial(downflag,Keyboard::Atari,shift,control);
    break;
  case XK_Menu:
  case XK_F1:
    if (downflag) machine->LaunchMenu() = true;
    break;
  case XK_F2:
    keyboard->HandleSpecial(downflag,Keyboard::Option,shift,control);
    break;
  case XK_F3:
    keyboard->HandleSpecial(downflag,Keyboard::Select,shift,control);
    break;
  case XK_F4:
    keyboard->HandleSpecial(downflag,Keyboard::Start,shift,control);
    break; 
  case XK_F5:
  case XK_Help:
    keyboard->HandleSpecial(downflag,Keyboard::Help,shift,control);
    break;
  case XK_F6:
    if (downflag) machine->WarmReset();
    break;
  case XK_F7:
    if (downflag) machine->ColdReset() = true;
    break;
  case XK_F8:
  case XK_Break:
  case XK_Cancel:
    keyboard->HandleSpecial(downflag,Keyboard::Break,shift,control);
    break;
  case XK_F9:
  case XK_Print:
    // Request a screen dump the next time.
    if (downflag) dump = true;
    break;
  case XK_F10:
    if (downflag) machine->Quit() = true;
    break;
  case XK_F11:
  case XK_Pause:
    if (downflag) machine->Pause() = !(machine->Pause());
    break;
  case XK_F12:
#ifdef BUILD_MONITOR
    if (downflag) machine->LaunchMonitor() = true;
#endif
    break;
  case XK_Home:
  case XK_Clear:
    if (control && shift) {
      keyboard->HandleSimpleKey(downflag,'<',true,true); // clear screen
    } else {
      keyboard->HandleSimpleKey(downflag,'<',false,true); // clear screen
    }
    break;
  case XK_Insert:
    if (shift) {
      keyboard->HandleSimpleKey(downflag,'>',true,control); // insert line
    } else {
      keyboard->HandleSimpleKey(downflag,'>',false,true); // insert character
    }
    break;
  case XK_BackSpace :
  case XK_Terminate_Server:
    keyboard->HandleSimpleKey(downflag,0x08,shift,control); // backspace
    break;
  case XK_Delete:
    keyboard->HandleSimpleKey(downflag,0x08,shift,!control); // delete
    break;
  case XK_Left:
    keyboard->HandleSimpleKey(downflag,'+',shift,!control); // cursor left
    break;
  case XK_Right:
    keyboard->HandleSimpleKey(downflag,'*',shift,!control); // cursor right
    break;
  case XK_Up:
    keyboard->HandleSimpleKey(downflag,'-',shift,!control); // cursor up
    break;
  case XK_Down:
    keyboard->HandleSimpleKey(downflag,'=',shift,!control); // cursor up
    break;
  case XK_Escape :
    keyboard->HandleSimpleKey(downflag,0x1b,shift,control);
    break;
  case XK_Tab:
#ifdef XK_ISO_Left_Tab
  case XK_ISO_Left_Tab: // used for Shift+Tab 
#endif
    keyboard->HandleSimpleKey(downflag,0x09,shift,control);
    break;
  case XK_Linefeed:
  case XK_Return:
    keyboard->HandleSimpleKey(downflag,0x0a,shift,control);
    break;
  }
}
///

/// X11_FrontEnd::HandleButtonEvent
// Handle a mouse button event.
void X11_FrontEnd::HandleButtonEvent(XButtonEvent *event)
{
  switch(event->button) {
  case 4:
    // A scroll-up event.
    scrolledlines--;
    break;
  case 5:
    // A scroll-down event.
    scrolledlines++;
    break;
  case 1:
  case 2:
    // Button events.
    if (event->type == ButtonPress) {
      button = true;
    } else if (event->type == ButtonRelease) {
      button = false;
    }
  }
}
///

/// X11_FrontEnd::ScrollDistance
// Return the amount of scrolled units by the scroll-wheel of the mouse.
int X11_FrontEnd::ScrollDistance(void)
{
  int lines     = scrolledlines;

  scrolledlines = 0;

  return lines;
}
///

/// X11_FrontEnd::VBI
// Implementation of the VBI activity required to drive the display frontend
// This is called frequently (in best case every 20 msecs) to drive and
// refresh the display
void X11_FrontEnd::VBI(class Timer *,bool quick,bool pause)
{
  bool grab;
  //
  if (!isinit)
    return;  
  //
  if (fixerror) {
    CloseDisplay();
    fixerror = false;
    throw AtariException("Spurious X11 Error","X11_FrontEnd::VBI","invalid X11 request detected");
    return;
  }
  //
  if (quick == false) {
    // We are not in a hurry. Update the display now.
    // Try differential update if possible (will be not on exposure events)
    GetFrameBuffer()->RebuildScreen(true);
    if (pause && ismapped)
      GetFrameBuffer()->HandleExposure();
  }
  // Now run the X event loop.
  HandleEventQueue();
  //
  // If screen dumping is enabled and we do not pause and we are not
  // on a hurry, make a dump. (-;
  if (pause == false && quick == false && dump == true) {
    DumpScreen();
    //
    // We're done with it.
    dump = false;
  }
  // If we are not pausing, advance the screen buffer to make room for
  // the next update.
  if (pause == false && quick == false) {
    // We do not advance the buffer if we need to be quick. This way,
    // "last" remains always the last usable buffer
    GetFrameBuffer()->NextBuffer();
  }
  //
  // Transmit the state of the emulated keyboard joystick.
  if (KeypadStick)
    KeypadStick->TransmitStates(pause);
  //
  // Transmit the state of the emulated mouse joystick
  grab  = MouseStick.TransmitStates(display,window,emuwidth,emuheight,pause);
  grab |= RelMouseStick.TransmitStates(display,window,emuwidth,emuheight,pause);
  // Possibly confine the mouse into the window now.
  GrabMouse(grab); 
  //
  // Possibly disable DPMS
#ifdef USE_DPMS
  /*
  if (DisableDPMS)
  enableMonitor(display);
  */
#endif    
}
///

/// X11_FrontEnd::MouseIsAvailable
// For GUI frontends: Check whether the mouse is available as input
// device. This returns false in case the mouse is an input device
// and has been grabbed on its "GamePort".
bool X11_FrontEnd::MouseIsAvailable(void)
{
  if (isgrabbed) {
    return false; // if grabbed, then it is not availble
  } else {
    return true;
  }
}
///

/// X11_FrontEnd::GrabMouse
// Confine the mouse into the active window if this is allowed.
void X11_FrontEnd::GrabMouse(bool grabme)
{
  if (grabme != isgrabbed) {
    // We need to do something about it
    if (grabme) {
      // We need to grab the mouse
      if (XGrabPointer(display,window,true,0,GrabModeAsync,GrabModeAsync,
		       window,None,CurrentTime) == GrabSuccess) {
	isgrabbed = true;
	// We also need to take the keyboard focus as we can't really control
	// the window otherwise.
	XSetInputFocus(display,window,RevertToNone,CurrentTime);
      }
    } else {
      XUngrabPointer(display,CurrentTime);
      isgrabbed = false;
    }
  }
}
///

/// X11_FrontEnd::EnforceFullRefresh
// Enforce a full display refresh, i.e. after the monitor returnes
void X11_FrontEnd::EnforceFullRefresh(void)
{      
  if (display && window)
    GetFrameBuffer()->HandleExposure();
}
///

/// X11_FrontEnd::SwitchScreen
// Enforce the screen to foreground or background to run the monitor
void X11_FrontEnd::SwitchScreen(bool foreground)
{
  if (display && window) {
    if (foreground) {
      XRaiseWindow(display,window);
    } else {
      XLowerWindow(display,window);
    }
  }
}
///

/// X11_FrontEnd::ShowPointer
// Enable or disable the mouse pointer
void X11_FrontEnd::ShowPointer(bool showit)
{  
  if (isinit) {
    XDefineCursor(display,window,(showit)?(Shown):(Hidden));
  }
  showcursor = showit;
}
///

/// X11_FrontEnd::MousePosition
// For GUI frontends within this buffer: Get the position and the status
// of the mouse within this buffer measured in internal coordinates.
void X11_FrontEnd::MousePosition(LONG &x,LONG &y,bool &but)
{   
  bool dummy;
  //
  // Initialize the frontend if we do not have done so already
  if (!isinit) {
    CreateDisplay();
  } 
  GetFrameBuffer()->MousePosition(x,y,dummy);
  //
  // Get the button state rather from the event chain and make sure
  // we did not miss any mouse-up events.
  if (dummy == false)
    button = false;
  but = button;
}
///

/// X11_FrontEnd::SetMousePosition
// Set the mouse position to the indicated coordinate
void X11_FrontEnd::SetMousePosition(LONG x,LONG y)
{
  if (isinit) {
    GetFrameBuffer()->SetMousePosition(x,y);
  }
}
///

/// X11_FrontEnd::MouseMoveStick::TransmitStates
// Query the current mouse status and transmit it to
// the game controllers connected to this port. Returns
// true if we need to obtain control over the mouse, i.e.
// have to grab it.
bool X11_FrontEnd::MouseMoveStick::TransmitStates(Display *d,Window w,
						  int width,int height,bool paused)
{
  WORD dx,dy;
  bool button1,button2;
  //
  // Do nothing if we feed no data
  if (ControllerChain().IsEmpty())
    return false; // Do not grab it.
  //
  //
  // Default is to have the button un-pressed and the mouse centered.
  dx = 0, dy = 0;
  button1 = false;
  button2 = false;
  if (paused == false) {
    int rootx,rooty;
    int winx,winy;
    unsigned int mask;
    Window activeroot,activechild;
    //  
    // Ok, query the mouse state from the X server now
    if (XQueryPointer(d,w,&activeroot,&activechild,
		      &rootx,&rooty,&winx,&winy,
		      &mask)) {
      // Only handle if the active screen is our screen.
      // First, check the button state. If buttons 1 or 2
      // are pressed, take this as a button down.
      button1 = (mask & Button1Mask)?true:false;
      button2 = (mask & Button3Mask)?true:false;
      //
      if (isrel) {
	int dxl,dyl;
	dxl = (winx - lastx) << 10;
	dyl = (winy - lasty) << 10;
#if HAVE_GETTIMEOFDAY
	struct timeval tv;
	if (gettimeofday(&tv,NULL) == 0) {
	  int deltasec  = tv.tv_sec  - lastsec;
	  int deltausec = tv.tv_usec - lastusec;
	  
	  lastsec  = tv.tv_sec;
	  lastusec = tv.tv_usec;
	  
	  if (deltasec >= 0) {
	    if (deltasec < (1L << 31) / (1000 * 1000)) {
	      int deltatime = deltasec * 1000 * 1000 + deltausec;
	      if (deltatime > 0) {
		dxl = (((winx - lastx) << 24) / deltatime);
		dyl = (((winy - lasty) << 24) / deltatime);
	      }
	    }
	  }
	}
#endif	
	//
	lastx   = winx;
	lasty   = winy;
	//
	// Avoid erratic non-movement due to no X-event on
	// movement coming in.
	if (dxl == 0 && dyl == 0 && (lastdx || lastdy)) {
	  dxl = lastdx;
	  dyl = lastdy;
	  lastdx = 0;
	  lastdy = 0;
	} else {
	  lastdx = dxl;
	  lastdy = dyl;
	}
	//
	dx      = (dxl > 32767)?(32767):((dxl < -32767)?(-32767):(dxl));
	dy      = (dyl > 32767)?(32767):((dyl < -32767)?(-32767):(dyl));
	//
	if (winx < (width  >> 2) || winx > (width  - (width >> 2)) ||
	    winy < (height >> 2) || winy > (height - (height >> 2))) {
	  XWarpPointer(d,w,w,0,0,width,height,width >> 1,height >> 1);
	  lastx = width  >> 1;
	  lasty = height >> 1;
	}
      } else {
	// Check for out of range issues.
	if (winx < 0)      winx = 0;
	if (winx > width)  winx = width;
	if (winy < 0)      winy = 0;
	if (winy > height) winy = height;
	//
	// Scale movements such that the window extremes are the joystick
	// extremes.
	width  >>= 1;
	height >>= 1;
	//
	dx = WORD((winx - width ) * 32767 / width);
	dy = WORD((winy - height) * 32767 / height);
      }
    }
  }
  //
  // Now forward the result to all connected classes
    //
  // Now feed all joystics on this port with the new and updated data.
  FeedAnalog(dx,dy);
  FeedButton(button1,0);
  FeedButton(button2,1);
    //
  // Whether we need to grab the mouse depends on whether we pause or not
  return !paused;
}
///

/// X11_FrontEnd::DumpScreen
// Make a screen dump of the currently active frame
void X11_FrontEnd::DumpScreen(void)
{
  FILE *dumpout;
  char buf[256];
  const char *annex;

  switch(Format) {
  case ScreenDump::PNM:
    annex = "ppm";
    break;
  case ScreenDump::BMP:
    annex = "bmp";
    break;
#ifdef USE_PNG
  case ScreenDump::PNG:
    annex = "png";
    break;
#endif
  default:
    annex = NULL; // shut-up the compiler
    Throw(InvalidParameter,"X11_FrontEnd::DumpScreen",
	  "invalid file format requested for the screen dump");
  }

  snprintf(buf,255,"%s_%d.%s",PictureBaseName,dumpcnt++,annex);

  dumpout = fopen(buf,"wb");
  if (dumpout) {
    try {
      GetFrameBuffer()->DumpScreen(dumpout,Format);
      fclose(dumpout);
    } catch(...) {
      // Shut up the temporary file, at least. We don't expect
      // that the Os cleans up behind us.
      fclose(dumpout);
      throw;
    }
  } else {  
    throw(AtariException(errno,"X11_FrontEnd::DumpScreen",
			 "Unable to open screen dump output file %s.",buf));
  }
}
///

/// X11_FrontEnd::SetLED
// If we want to emulate a flickering drive LED, do it here.
void X11_FrontEnd::SetLED(bool /*on*/)
{
  /*
  static const int USE_LED = 3;
  
  XKeyboardControl control={0, 0, 0, 0,
			    1<<(USE_LED-1), (on ? LedModeOn : LedModeOff),
			    0, 0};
  
  XChangeKeyboardControl(display, KBLed | KBLedMode, &control);
  */
}
///

/// X11_FrontEnd::ActiveBuffer
// Return the active buffer we must render into
UBYTE *X11_FrontEnd::ActiveBuffer(void)
{
  // Initialize the frontend if we do not have done so already
  if (!isinit) {
    CreateDisplay();
  }
  // 
  // Ask the framebuffer about it, we don't know anything...
  return GetFrameBuffer()->ActiveBuffer();
}
///

/// X11_FrontEnd::ResetVertical
// Signal that we start the display generation again from top. Hence, this implements
// some kind of "vertical sync" signal for the display generation.
void X11_FrontEnd::ResetVertical(void)
{
  // Initialize the frontend if we do not have done so already
  if (!isinit) {
    CreateDisplay();
  }
  // 
  // Ask the framebuffer about it, we don't know anything...
  GetFrameBuffer()->ResetVertical();
}
///

/// X11_FrontEnd::ColdStart
void X11_FrontEnd::ColdStart(void)
{
  WarmStart();
}
///

/// X11_FrontEnd::WarmStart
void X11_FrontEnd::WarmStart(void)
{
  // Reset the keypad stick here.
  if (KeypadStick)
    KeypadStick->Reset();
}
///

/// X11_FrontEnd::ParseArgs
// Parse off command line arguments relevant for this class
void X11_FrontEnd::ParseArgs(class ArgParser *args)
{
  bool privatec = PrivateCMap;
  LONG pxwidth  = PixelWidth;
  LONG pxheight = PixelHeight;
  LONG le       = LeftEdge;
  LONG te       = TopEdge;
  LONG w        = Width;
  LONG h        = Height;
  LONG format   = Format;
  bool indirect = PixMapIndirect;
  bool xvideo   = EnableXVideo;
  static const struct ArgParser::SelectionVector Formats[] = {
    {"PNM", ScreenDump::PNM},
    {"BMP", ScreenDump::BMP},
#ifdef USE_PNG
    {"PNG", ScreenDump::PNG},
#endif
    {NULL , 0}
  };
  
  if (Unit == 0) {
    args->DefineTitle("X11_FrontEnd");
  } else {
    args->DefineTitle("XEP11_FrontEnd");
  }
  args->DefineBool("PrivateCMap","allocate a private colormap",PrivateCMap);
  args->DefineBool("SyncX","enforce synchronous X rendering",SyncX);
#ifdef USE_DPMS
  args->DefineBool("DisableDPMS","disable screen power saving",DisableDPMS);
#endif
#ifdef X_USE_XV
  args->DefineBool("XVideoRendering","render through XVideo extension",EnableXVideo);
#endif
  args->DefineBool("RenderIndirect","enable rendering thru a pixmap",PixMapIndirect);
  args->DefineString("ScreenBase","file base name for screen dumps",PictureBaseName);
  args->DefineLong("PixelWidth","sets the pixel width multiplier",1,8,PixelWidth);
  args->DefineLong("PixelHeight","sets the pixel height multiplier",1,8,PixelHeight);
  if (Unit == 0) {
    args->DefineLong("LeftEdge","set left edge of visible screen",0,64,LeftEdge);
    args->DefineLong("TopEdge","set top edge of visible screen",0,64,TopEdge);
    args->DefineLong("Width","set width of visible screen",
		     320,Antic::DisplayModulo,Width);
    args->DefineLong("Height","set height of visible screen",
		     192,Antic::DisplayHeight,Height);
  }
  args->DefineSelection("DumpFormat","screen dump gfx file format",Formats,format);

  Format = (ScreenDump::GfxFormat)format;
#ifdef USE_DPMS
  disableDPMS(display,DisableDPMS);
  if (fixerror && xvideo) {
    EnableXVideo = false;
    UnloadFrameBuffer();
    // Sync and discard all events on the event queue
    XSync(display,true);
    fixerror     = false; 
    args->SignalBigChange(ArgParser::Reparse);
    machine->PutWarning("XVideo display not available, disabling it.");
  }
#endif  
  if (pxwidth   != PixelWidth     ||
      pxheight  != PixelHeight    ||
      le        != LeftEdge       ||
      te        != TopEdge        ||
      w         != Width          ||
      h         != Height         ||
      indirect  != PixMapIndirect ||
      privatec  != PrivateCMap) {
    args->SignalBigChange(ArgParser::Reparse);
    CloseDisplay();
  }
  if (truecolor != machine->GTIA()->SuggestTrueColor() || xvideo != EnableXVideo   ||
      (colormap && (colormap != machine->GTIA()->ActiveColorMap()))) {
    args->SignalBigChange(ArgParser::Reparse);
    UnloadFrameBuffer();
    colormap = machine->GTIA()->ActiveColorMap();
  }
}
///

/// X11_FrontEnd::DisplayStatus
// Display the internal status of this class
void X11_FrontEnd::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("X11_FrontEnd status:\n"
		   "\tPrivateCMap           : %s\n"
		   "\tSyncX                 : %s\n"
		   "\tScreen dump base name : %s\n"
		   "\tIndirect rendering    : %s\n"
		   "\tPixel width           : " ATARIPP_LD "\n"
		   "\tPixel height          : " ATARIPP_LD "\n"
		   "\tTrue color display    : %s\n"
		   "\tLeftEdge              : " ATARIPP_LD "\n"
		   "\tTopEdge               : " ATARIPP_LD "\n"
		   "\tWidth                 : " ATARIPP_LD "\n"
		   "\tHeight                : " ATARIPP_LD "\n",
		   PrivateCMap?("on"):("off"),
		   SyncX?("on"):("off"),
		   PictureBaseName,		   
		   (PixMapIndirect)?("on"):("off"),
		   PixelWidth,PixelHeight,
		   (truecolor)?("on"):("off"),
		   LeftEdge,TopEdge,Width,Height);
}
///

/// X11_FrontEnd::EnableDoubleBuffer
// Disable or enable double buffering temporarely for the user
// menu. Default is to have it enabled.
void X11_FrontEnd::EnableDoubleBuffer(bool)
{
}
///

///
#endif // of if HAVE_LIBX11
///
