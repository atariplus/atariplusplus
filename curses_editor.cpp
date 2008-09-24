/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: curses_frontend.cpp,v 1.7 2008/05/25 18:17:30 thor Exp $
 **
 ** In this module: A full screen text editor using curses
 **********************************************************************************/

/// Includes
#include "curses_editor.hpp"
#ifdef USE_CURSES
///

/// Curses_Editor::Curses_Editor
Curses_Editor::Curses_Editor(class Machine *mach)
  : Chip(mach,"CursesEditor"), window(NULL)
{
}
///

/// Curses_Editor::~Curses_Editor
Curses_Editor::~Curses_Editor(void)
{
  ExitCurses();
}
///

/// Curses_Editor::WarmStart
void Curses_Editor::WarmStart(void)
{
}
///

/// Curses_Editor::ColdStart
void Curses_Editor::ColdStart(void)
{
}
///

/// Curses_Editor::InitCurses
void Curses_Editor::InitCurses(void)
{
  if (window == NULL) {
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

/// Curses_Editor::ExitCurses
// Cleanup the curses interface.
void Curses_Editor::ExitCurses(void)
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

/// Curses_Editor::GetExtendedKey
// Collect a keyboard key, return the extended
// keyboard description similar to the internal
// K: handler (extended codes). Returns -1 in
// case no key is available. Does not busy-wait.
int Curses_Editor::GetExtendedKey(void)
{
  int ch = getch();
  // A suitable input? Generates ERR in case
  // no input has been collected. Each key generates
  // two events: A key-down and immediately following
  // a key-up event.
  switch(ch) {
  case KEY_F(8):
  case KEY_BREAK:
  case KEY_CANCEL:
    return Break;
  case KEY_DOWN:
    return Down;
  case KEY_UP:
    return Up;
  case KEY_LEFT:
  case KEY_SLEFT:
    return Left;
  case KEY_RIGHT:
  case KEY_SRIGHT:
    return Right;
  case KEY_HOME:
  case KEY_BEG:
  case KEY_SHOME:
    return Home;
  case KEY_LL:
  case KEY_END:
  case KEY_SEND:
    return End;
  case KEY_BACKSPACE:
    return Backspace;
  case KEY_F(1):
    return F1;
  case KEY_F(2):
    return F2;
  case KEY_F(3):
    return F3;
  case KEY_F(4):
    return F4;
  case KEY_HELP:
  case KEY_F(5):
    return Help;
  case KEY_SHELP:
    return SHelp;
  case KEY_F(6):
  case KEY_SRESET:
    machine->WarmReset();
    break;
  case KEY_F(7):
  case KEY_RESET:
    machine->ColdReset() = true;
    break;
    // Dump or print are not supported here.
  case KEY_F(10):
    machine->Quit() = true;
    break;
  case KEY_F(11):
  case KEY_RESUME: 
  case KEY_SUSPEND:
    return Stop;
  case KEY_F(12):
#ifdef BUILD_MONITOR
    machine->LaunchMonitor() = true;
#endif
    break;
  case KEY_IC:
    return InsertChar;
  case KEY_DC:
  case 0x7f: // Also DEL
    return DeleteChar;
  case KEY_DL:
  case KEY_PPAGE:
  case KEY_SDC:
  case KEY_SDL:
    return DeleteLine;
  case KEY_IL:
  case KEY_NPAGE:
  case KEY_SIC:
    return InsertLine;
  case KEY_CLEAR:
    return Clear;
  case KEY_STAB:
    return InsertTab;
  case KEY_CTAB:
    return DeleteTab;
  case 0x1b: // Escape.
    return Escape;
  case 0x09: // TAB
    return Tab;
  case KEY_ENTER:
  case 0x0a:
    return EOL;
  case '~': // Not available at the Atari anyhow, used to simulate caps.
    return EOF;
  case ERR: // No input at all.
    return -1;
  default:
    if (ch < 0x80) {
      return ch;
    }
    return -1;
  }
}
///

/// Curses_Editor::ParseArgs
// Parse up the command line arguments
void Curses_Editor::ParseArgs(class ArgParser *args)
{
}
///

///
#endif
