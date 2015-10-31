/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: stdio.hpp,v 1.7 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Os compatibility layer for stdio management.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them.
 **********************************************************************************/

#ifndef STDIO_HPP
#define STDIO_HPP

/// Includes
#include "types.h"
#include <stdio.h>
#if HAVE_STDARG_H
#include <stdarg.h>
#else
#error "requires the stdarg.h header"
#endif
///

/// Replacements for snprintf and vsnprintf
#if !HAVE_SNPRINTF
int snprintf(char *str, size_t size, const char *format, ...) PRINTF_STYLE;
#elif defined( _WIN32 )
#define snprintf _snprintf
#endif
///

/// Replacements for vsnprintf
#if !HAVE_VSNPRINTF
int vsnprintf(char *str, size_t size, const char *format,va_list ap);
#elif defined( _WIN32 )
#define vsnprintf _vsnprintf
#endif
///

/// Explicit console opener for win32
#if MUST_OPEN_CONSOLE
extern void OpenConsole();
extern void CloseConsole();
#endif
///

///
#endif
