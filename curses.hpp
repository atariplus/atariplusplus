/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: curses.hpp,v 1.1 2003-02-23 00:40:32 thor Exp $
 **
 ** In this module: Curses abstraction layer
 **********************************************************************************/

#ifndef CURSES_HPP
#define CURSES_HPP

/// Includes
#include "types.h"

// Some architectures (IRIX) do not contain the proper guards for C++
// includes. We add them here.
extern "C" {
#if HAVE_INITSCR && HAVE_HALFDELAY
#if HAVE_NCURSES_H
// Some broken architectures (SuSe Linux 8.0) do not come with a working
// compatibility layer for curses. Therefore, try to include ncurses first
// and fall back to curses otherwise. On these architectures, ncurses also
// requires the obscure ncurses_dll.h include. Icky.
#ifdef HAVE_NCURSES_DLL_H
#include <ncurses_dll.h>
#endif
#define USE_CURSES 1
#include <ncurses.h>
#elif HAVE_CURSES_H
#define USE_CURSES 1
#include <curses.h>
#endif
#endif
}
///

///
#endif
