/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: stdio.cpp,v 1.5 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Os compatibility layer for stdio management.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them.
 **********************************************************************************/

/// Includes
#include "string.hpp"
#include "stdio.hpp"
#if HAVE_STDARG_H
#include <stdarg.h>
#else
#error "requires the stdarg.h header"
#endif
///

/// Replacements for snprintf and vsnprintf
#if !HAVE_SNPRINTF
int snprintf(char *str, size_t size, const char *format, ...)
{
  int r;
  va_list args;

  va_start(args,format);
  r = vsnprintf(str,size,format,args);
  va_end(args);

  return r;
}
#endif
///

/// Replacements for vsnprintf
#if !HAVE_VSNPRINTF
int vsnprintf(char *str, size_t size, const char *format,va_list ap)
{
  if (size < 2048) {
    size_t len;
    static char buffer[2048]; // hope this is enough.
    // Since the Os does not support it, we forget about overflows. Huh...
    vsprintf(buffer,format,ap);
    len = strlen(buffer);
    // Terminate this string, one way or another
    if (size) {
      buffer[size-1] = '\0';
    }
    strcpy(str,buffer);
    return (int)(len);
  } else {
    return vsprintf(str,format,ap);
  }
}
#endif
///
