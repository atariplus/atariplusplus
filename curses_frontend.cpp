/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: curses_frontend.cpp,v 1.12 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: A frontend using the curses library for text output
 **********************************************************************************/

/// Includes
#include "curses.hpp"
#ifdef USE_CURSES
#include "string.hpp"
#include "curses_frontend.hpp"
#include "antic.hpp"
#include "mmu.hpp"
#include "adrspace.hpp"
#include "keyboard.hpp"
#include "monitor.hpp"
///

/// Defines
// Increment the antic display counter, and do not cross a 1K boundary.
#define INCPC(AnticPC,d)  AnticPC = ((AnticPC + d) & 0x03ff) | (AnticPC & 0xfc00)
///

/// Curses_FrontEnd::Curses_FrontEnd
// Construct the object
Curses_FrontEnd::Curses_FrontEnd(class Machine *mach)
  : AtariDisplay(mach,0), LastDisplayBuffer(NULL), DisplayBuffer(NULL),
    InputBuffer(new UBYTE[Antic::DisplayModulo]),
    window(NULL), Antic(NULL), MMU(NULL), keyboard(NULL),
    fullrefresh(true), sendcaps(true), 
    optioncnt(0), selectcnt(0), startcnt(0),
    KeypadStick(mach)
{
}
///

/// Curses_FrontEnd::~Curses_FrontEnd
Curses_FrontEnd::~Curses_FrontEnd(void)
{
  ExitCurses();
  delete[] LastDisplayBuffer;
  delete[] DisplayBuffer;
  delete[] InputBuffer;
}
///

/// Curses_FrontEnd::ColdStart
// Run a cold-start for this class, re-initialize all pointers
void Curses_FrontEnd::ColdStart(void)
{
}
///

/// Curses_FrontEnd::WarmStart
void Curses_FrontEnd::WarmStart(void)
{ 
  Antic = machine->Antic();
  MMU   = machine->MMU();
  // Get the keyboard from the machine.
  keyboard = machine->Keyboard();
  if (LastDisplayBuffer == NULL)
    LastDisplayBuffer = new char[BufferWidth * BufferHeight];
  memset(LastDisplayBuffer,' ',BufferWidth * BufferHeight);
  if (DisplayBuffer == NULL)
    DisplayBuffer = new char[BufferWidth * BufferHeight];
  memset(DisplayBuffer,    ' ',BufferWidth * BufferHeight);
  fullrefresh = true;
  sendcaps    = true;
  KeypadStick.Reset();
}
///

/// Curses_FrontEnd::InitCurses
// Start the curses front-end/display
void Curses_FrontEnd::InitCurses(void)
{  
  if (window == NULL) {
    // Get linkage.
    WarmStart();
    //
    window = initscr();
    clearok(window,true);
    nl();
    noecho();
    //cbreak();
    curs_set(0);
    raw();
    refresh();
    keypad(((WINDOW *)window),true);
    scrollok((WINDOW*)window,false);
    idlok((WINDOW*)window,false);
    nodelay((WINDOW*)window,true);
  }
}
///  

/// Curses_FrontEnd::ExitCurses
// Disable the curses front-end/display
void Curses_FrontEnd::ExitCurses(void)
{
  if (window) {
    curs_set(1);  
    nocbreak();
    echo();
    endwin();
    window = NULL;
  }
}
///

/// Curses_FrontEnd::AnalyzeDisplay
// Analyze the display and copy it into the current
// display buffer.
void Curses_FrontEnd::AnalyzeDisplay(void)
{
  class AdrSpace *adr = MMU->AnticRAM(); // get the memory antic uses for display
  ADR display = 0; // start of the display memory buffer. Hopefully reloaded by antic.
  ADR dlist   = Antic->DisplayList();
  int row     = 0; // row we are currently analyizing. At most BufferHeight rows are checked.
  int lines   = 0; // current display line. Used here to avoid pathological cases. At most 256 lines.
  int width   = Antic->CharacterWidth(); // Number of characters to fill.
  int ir      = 0x41; // next antic instruction. Default: End of playfield.
  char *buf   = DisplayBuffer;
  char *target;
  ADR src;
  int i;

  while(row < BufferHeight && lines < 256) {
    // Any data to fetch here?
    if (width) {
      // Get next instruction.
      ir = adr->ReadByte(dlist);
      //
      // If this is the end of the display, set the width to zero to fill the rest
      // of the screen with empty lines.
      if (ir == 0x41) {
	width = 0;
	continue;
      }
      //
      INCPC(dlist,1);
      // If this an Antic jump instruction, jump and continue with the next
      // instruction, if this is a blank instruction, don't do anything, just
      // count the lines.
      if ((ir & 0x0f) == 0x00) {
	lines += ((ir >> 4) & 0x07) + 1;
	continue;
      } else if ((ir & 0x0f) == 0x01) {
	// Jumping around. Costs one line, but no row on the curses display.
	lines++;
	dlist = adr->ReadWord(dlist);
	continue;
      }
      //
      // Check whether the scan pointer is to be reloaded.
      if (ir & 0x40) {
	display = adr->ReadWord(dlist);
	INCPC(dlist,2);
      }
      //
      // Now check for the display mode. Only a couple of modes are actually supported.
      switch(ir & 0x0f) {
      case 5:
	lines += 6; // plus eight with the +2 below.
	// Falls through.
      case 3:
	lines += 2;
	// Falls through.
      case 2:
      case 4:
	lines += 8;
	// 8x8 and 8x10 character cells, both handled alike here.
	memset(buf,' ',BufferWidth);
	// Compute the position into which the first character goes.
	target = buf + ((BufferWidth - width) >> 1);
	// Get the source. This is the default source, unless horizontal scrolling
	// comes into play.
	if (ir & 0x10) {
	  src     = display + 4 - ((Antic->HScrollOffset() & 0x0f) >> 2);
	  display = (display & 0xf000) | ((display + ((width == 48)?(48):(width + 8))) & 0x0fff);
	} else {
	  src     = display;
	  display = (display & 0xf000) | ((display +  width) & 0x0fff);
	}
	//
	for(i = 0;i < width;i++) {
	  int out,c;
	  //
	  c   = adr->ReadByte(src);
	  // Convert the character to the ASCII set
	  out = (c & 0x80);
	  switch(c & 0x60) {
	  case 0x00:
	    out |= UBYTE(0x20 | (c & 0x1f));
	    break;
	  case 0x20:
	    out |= UBYTE(0x40 | (c & 0x1f));
	    break;
	  case 0x40:
	    out |= UBYTE(0x00 | (c & 0x1f));
	    break;
	  case 0x60:
	    out |= UBYTE(0x60 | (c & 0x1f));
	    break;
	  }
	  *target++ = out;
	  src       = (src & 0xf000) | ((src + 1) & 0x0fff);
	}
	// Another row successfully handled.
	row++;
	buf += BufferWidth;
	//
	// If this is mode #5, clean the next line as well if it is available,
	// lines are double-high in this mode.
	if ((ir & 0x0f) == 5 && row < BufferHeight) {
	  memset(buf,' ',BufferWidth);
	  row++;
	  buf += BufferWidth;
	}
	break;
      case 7:
	lines += 8;
	// Falls through.
      case 6:
	lines += 8;
	// 8x8 and 8x16 character cells, both handled alike here, but pixels are one
	// color clock high here.
	memset(buf,' ',BufferWidth);
	// Compute the position into which the first character goes.
	target = buf + ((BufferWidth - width) >> 1);
	// Get the source. This is the default source, unless horizontal scrolling
	// comes into play.
	if (ir & 0x10) {
	  src     = display + 2 - ((Antic->HScrollOffset() & 0x0f) >> 3);
	  display = (display & 0xf000) | ((display + ((width == 48)?(24):((width + 8) >> 1))) & 0x0fff);
	} else {
	  src     = display;
	  display = (display & 0xf000) | ((display + (width >> 1)) & 0x0fff);
	}
	//
	for(i = 0;i < width;i += 2) {
	  int out,c;
	  //
	  c   = adr->ReadByte(src);
	  // Convert the character to the ASCII set, only upper case here.
	  out = (c & 0x80);
	  switch(c & 0x60) {
	  case 0x00:
	  case 0x40:
	    out |= UBYTE(0x20 | (c & 0x1f));
	    break;
	  case 0x20:
	  case 0x60:
	    out |= UBYTE(0x40 | (c & 0x1f));
	    break;
	  }
	  *target++ = out;
	  // Emulate the double-wide characters.
	  *target++ = ' ' | (out & 0x80);
	  src       = (src & 0xf000) | ((src + 1) & 0x0fff);
	}
	// Another row successfully handled.
	row++;
	buf += BufferWidth;
	//
	// If this is mode #7, clean the next line as well if it is available,
	// lines are double-high in this mode.
	if ((ir & 0x0f) == 7 && row < BufferHeight) {
	  memset(buf,' ',BufferWidth);
	  row++;
	  buf += BufferWidth;
	}
	break;
      case 8:
	lines += 4;
	// Falls through.
      case 9:
	lines += 4;
	if (ir & 0x10) {
	  display = (display & 0xf000) | ((display + ((width == 48)?(12):((width + 8) >> 2))) & 0x0fff);
	} else {
	  display = (display & 0xf000) | ((display + (width >> 2)) & 0x0fff);
	}
	break;
      case 10:
	lines += 2;
	// Falls through.
      case 11:
	lines += 1;
	// Falls through.
      case 12:
	lines += 1;
	if (ir & 0x10) {
	  display = (display & 0xf000) | ((display + ((width == 48)?(24):((width + 8) >> 1))) & 0x0fff);
	} else {
	  display = (display & 0xf000) | ((display + (width >> 1)) & 0x0fff);
	}
	break;	
      case 13:
	lines += 1;
	// Falls through.
      case 14:
      case 15:
	lines += 1;
	if (ir & 0x10) {
	  display = (display & 0xf000) | ((display + ((width == 48)?(24):((width + 8) >> 1))) & 0x0fff);
	} else {
	  display = (display & 0xf000) | ((display + (width >> 1)) & 0x0fff);
	}
	break;
      }
    } else {
      // No width. Just fill the buffer.
      memset(buf,' ',BufferWidth);
      row++;
      buf += BufferWidth;
    }
  }
  //
  // If any lines are left, fill the rest with blanks.
  while(row < BufferHeight) {
    memset(buf,' ',BufferWidth);
    row++;
    buf += BufferWidth;
  }
}
///

/// Curses_FrontEnd::RedrawScreen
// Draw the contents of the screen buffer to the curses output
void Curses_FrontEnd::RedrawScreen(void)
{
  int x,y;
  char *buf  = DisplayBuffer;
  char *last = LastDisplayBuffer;
  int w,h;

#ifdef HAS_COLS
  w = COLS;
  if (w > BufferWidth)
#endif
    w = BufferWidth;
  
#ifdef HAS_LINES
  h = LINES;
  if (h > BufferHeight)
#endif
    h = BufferHeight;

  for(y = 0;y < h;y++) {
    for(x = 0;x < w;x++) {
      if (fullrefresh || *buf != *last) {
	chtype out = 0;
	char ch    = *buf;
	*last      = *buf;
	//
	// Set reverse video or not.
	if (ch & 0x80) {
	  out |= A_REVERSE;
	  ch  &= 0x7f;
	}
	//
	// Filter special ATASCII characters.
	switch(ch) {
	case 0x00:
	case 0x09:
	case 0x0b:
	case 0x0c:
	case 0x0f:
	case 0x14:
	  out |= ACS_BULLET;
	  break;
	case 0x01:
	  out |= ACS_LTEE;
	  break;
	case 0x02:
	case 0x16:
	case 0x19:
	  out |= ACS_VLINE;
	  break;
	case 0x03:
	  out |= ACS_LRCORNER;
	  break;
	case 0x04:
	  out |= ACS_RTEE;
	  break;
	case 0x05:
	  out |= ACS_URCORNER;
	  break;
	case 0x06:
	case 0x08:
	  out |= '/';
	  break;
	case 0x07:
	case 0x0a:
	  out |= '\\';
	  break;
	case 0x0d:
	  out |= ACS_S1;
	  break;
	case 0x0e:
	case 0x15:
	  out |= ACS_S9;
	  break;
	case 0x10:
	case 0x7b:
	  out |= ACS_DIAMOND;
	  break;
	case 0x11:
	  out |= ACS_ULCORNER;
	  break;
	case 0x12:
	  out |= ACS_HLINE;
	  break;
	case 0x13:
	  out |= ACS_PLUS;
	  break;
	case 0x17:
	  out |= ACS_TTEE;
	  break;
	case 0x18:
	  out |= ACS_BTEE;
	  break;
	case 0x1a:
	  out |= ACS_LLCORNER;
	  break;
	case 0x1b:
	  out |= ACS_CKBOARD;
	  break;
	case 0x1c:
	  out |= ACS_UARROW;
	  break;
	case 0x1d:
	  out |= ACS_DARROW;
	  break;
	case 0x1e:
	case 0x7e:
	  out |= ACS_LARROW;
	  break;
	case 0x1f:
	case 0x7f:
	  out |= ACS_RARROW;
	  break;
	case 0x7d:
	  out |= ACS_LARROW;
	  break;
	default:
	  out |= ch;
	  break;
	}
	mvaddch(y,x,out);
      }
      buf++,last++;
    }
  }

  fullrefresh = false;
  refresh();
}
///

/// Curses_FrontEnd::HandleEventQueue
// Handle keyboard input - the only type of event
// ncurses can create.
void Curses_FrontEnd::HandleEventQueue(void)
{
  bool control = false;
  //bool shift   = false;
  //
  // Check whether the console keys go up again.
  if (startcnt > 0) {
    if (--startcnt == 0)
      keyboard->HandleSpecial(false,Keyboard::Start,false,false);
  }
  if (selectcnt > 0) {
    if (--selectcnt == 0)
      keyboard->HandleSpecial(false,Keyboard::Select,false,false);
  }
  if (optioncnt > 0) {
    if (--optioncnt == 0)
      keyboard->HandleSpecial(false,Keyboard::Option,false,false);
  }
  //
  // In case we got a reset-signal, send the Caps character to
  // get lower-case keys.
  if (sendcaps) {
    keyboard->HandleSpecial(true ,Keyboard::Caps,false,false);
    keyboard->HandleSpecial(false,Keyboard::Caps,false,false);
    sendcaps = false;
  }
  //
  int ch = getch();
  // A suitable input? Generates ERR in case
  // no input has been collected. Each key generates
  // two events: A key-down and immediately following
  // a key-up event.
  switch(ch) {
  case KEY_F(8):
  case KEY_BREAK:
    keyboard->HandleSpecial(true,Keyboard::Break,false,false);
    keyboard->HandleSpecial(false,Keyboard::Break,false,false);
    break;
  case KEY_DOWN:
    keyboard->HandleSimpleKey(true,'=',false,true); 
    keyboard->HandleSimpleKey(false,'=',false,true);
    break;
  case KEY_UP:
    keyboard->HandleSimpleKey(true,'-',false,true);
    keyboard->HandleSimpleKey(false,'-',false,true);
    break;
  case KEY_LEFT:
    keyboard->HandleSimpleKey(true,'+',false,true); 
    keyboard->HandleSimpleKey(false,'+',false,true);
    break;
  case KEY_RIGHT:
    keyboard->HandleSimpleKey(true,'*',false,true); 
    keyboard->HandleSimpleKey(false,'*',false,true);
    break;
  case KEY_HOME:
    keyboard->HandleSimpleKey(true,'<',false,true); // clear screen
    keyboard->HandleSimpleKey(false,'<',false,true);
    break;
  case KEY_BACKSPACE:
    keyboard->HandleSimpleKey(true,0x08,false,false); // backspace
    keyboard->HandleSimpleKey(false,0x08,false,false);
    break;
  case KEY_F(1):
    keyboard->HandleSpecial(true,Keyboard::Atari,false,false);
    keyboard->HandleSpecial(false,Keyboard::Atari,false,false);
    break;
  case KEY_F(2):
  case KEY_OPTIONS:
    keyboard->HandleSpecial(true,Keyboard::Option,false,false);
    optioncnt = 3;
    break;
  case KEY_F(3):
  case KEY_SELECT:
    keyboard->HandleSpecial(true,Keyboard::Select,false,false);
    selectcnt = 3;
    break;
  case KEY_F(4):
    keyboard->HandleSpecial(true,Keyboard::Start,false,false);
    startcnt  = 3;
    break;
  case KEY_F(5):
  case KEY_HELP:
    keyboard->HandleSpecial(true,Keyboard::Help,false,false);
    keyboard->HandleSpecial(false,Keyboard::Help,false,false);
    break;
  case KEY_F(6):
    machine->WarmReset();
    break;
  case KEY_F(7):
    machine->ColdReset() = true;
    break;
    // Dump or print are not supported here.
  case KEY_F(10):
    machine->Quit() = true;
    break;
  case KEY_F(11):
  case KEY_RESUME: 
  case KEY_SUSPEND:
    machine->Pause() = !(machine->Pause());
    break;
  case KEY_F(12):
#ifdef BUILD_MONITOR
    machine->LaunchMonitor() = true;
#endif
    break;
  case KEY_IC:
    keyboard->HandleSimpleKey(true,'>',false,true); // insert character
    keyboard->HandleSimpleKey(false,'>',false,true); // insert character
    break;
  case KEY_DC:
  case 0x7f: // Also DEL
    keyboard->HandleSimpleKey(true,0x08,false,true);
    keyboard->HandleSimpleKey(false,0x08,false,true);
    break;
  case 0x1b: // Escape.
  case 0x09: // TAB
    keyboard->HandleSimpleKey(true,ch,false,false);
    keyboard->HandleSimpleKey(false,ch,false,false);
    break;
  case KEY_ENTER:
  case 0x0a:
    keyboard->HandleSimpleKey(true,0x0a,false,false);
    keyboard->HandleSimpleKey(false,0x0a,false,false);
    break;
  case '~': // Not available at the Atari anyhow, used to simulate caps.
    keyboard->HandleSpecial(true ,Keyboard::Caps,false,false);
    keyboard->HandleSpecial(false,Keyboard::Caps,false,false);
    break;
  case ERR: // No input at all.
    break;
  default:
    if (ch <= 0x1f) {
      // A character pressed with Control.
      ch     |= 0x60;
      control = true;
    }
    if (ch < 0x80) {
      keyboard->HandleKey(true,ch,false,control);
      keyboard->HandleKey(false,ch,false,control);
    }
    break;
  }
}
///

/// Curses_FrontEnd::VBI
// Implement the frequent VBI activity of the display: This should
// update the display and read inputs
void Curses_FrontEnd::VBI(class Timer *,bool quick,bool)
{  
  // If curses is not yet running, start it now.
  if (fullrefresh) {
    ExitCurses();
  }
  if (window == NULL) {
    InitCurses();
  }
  //
  if (quick == false && Antic) {
    // Refresh the display here unless we're out of time.
    AnalyzeDisplay();
    RedrawScreen();
  }
  //
  // Now run the event loop.
  if (keyboard)
    HandleEventQueue();
}
///

/// Curses_FrontEnd::ParseArgs
// Parse off command line arguments.
void Curses_FrontEnd::ParseArgs(class ArgParser *)
{
}
///

/// Curses_FrontEnd::DisplayStatus
// Print the status.
void Curses_FrontEnd::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Curses_FrontEnd Status:\n"
		   "Front end installed and working.\n");
}
///

/// Curses_FrontEnd::SwitchScreen
// Enforce the screen to foreground or background to run the monitor
void Curses_FrontEnd::SwitchScreen(bool foreground)
{
  if (foreground) {
    fullrefresh = true;
  } else {
    ExitCurses();
  }
}
///

///
#endif
