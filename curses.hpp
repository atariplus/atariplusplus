/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: curses.hpp,v 1.5 2013/11/29 13:11:05 thor Exp $
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
#include <stdio.h> // include this to enable the SGI workaround.
#ifdef __SGI_LIBC_END_EXTERN_C
# ifdef __STRICT_ANSI__
#  define TMP_STRICT_ANSI __STRICT_ANSI__
#  undef __STRICT_ANSI__
# endif
# ifdef __USE_FIXED_PROTOTYPES__
#  define TMP_FIXED_PROTOS __USE_FIXED_PROTOTYPES__
#  undef __USE_FIXED_PROTOTYPES__
# endif
# define TMP_cplusplus __cplusplus
# undef __cplusplus
#endif
#include <curses.h>
#ifdef __SGI_LIBC_END_EXTERN_C
# define __cplusplus TMP_cplusplus
# ifdef TMP_STRICT_ANSI
#  define __STRICT_ANSI__ TMP_STRICT_ANSI
# endif
# ifdef TMP_FIXED_PROTOS
#  define __USE_FIXED_PROTOTYPES__ TMP_FIXED_PROTOS
# endif
#endif
#endif
#endif
}
///

///
#endif
