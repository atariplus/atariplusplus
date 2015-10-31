/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: string.hpp,v 1.10 2015/11/07 18:53:12 thor Exp $
 **
 ** In this module: Os compatibility layer for string management.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them.
 **********************************************************************************/

#ifndef STRING_HPP
#define STRING_HPP

/// Includes
#include "types.h"

#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif
///

/// Check for memchr function and provide a replacement if we don't have it.
#if !HAVE_MEMCHR
inline static void *memchr(const void *s, int c, size_t n) throw()
{
  const unsigned char *in = (const unsigned char *)s;

  if (n) {
    do {
      if (*in == c) return const_cast<void *>(s);
      in++;
    } while(--n);
  }
  return 0;
}
#endif
///

/// Check for the memmove function and provide a replacement if we don't have it.
#if !HAVE_MEMMOVE
// This is not overly efficient, but it should at least work...
inline static void *memmove(void *dest, const void *src, size_t n) throw()
{
  unsigned char       *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;

  if (n) {
    if (s > d) {
      // Source is behind dest, copy ascending.
      do {
	*d = *s;
	d++,s++;
      } while(--n);
    } else if (s < d) {
      // Source is in front of dest, copy descending.
      s += n, d += n;
      do {
	--d,--s;
	*d = *s;
      } while(--n);
    }
  }
  return dest;
}
#endif
///

/// Check for the memset function and provide a replacement if we don't
#if !HAVE_MEMSET
inline static void *memset(void *s, int c, size_t n) throw()
{
  unsigned char *d = (unsigned char *)s;
  
  if (n) {
    do {
      *d = c;
      d++;
    } while(--n);
  }

  return const_cast<void *>(s);
}
#endif
///

/// Check for availibility of strcasecmp and implement it if it does not exist.
#ifdef _WIN32
# define strcasecmp(a,b) stricmp((a),(b))
#endif
#if !HAVE_STRCASECMP
#include <ctype.h>
inline static int strcasecmp(const char *s1, const char *s2) throw()
{
  int d;
  
  while(*s1) {
    d = toupper(*s1) - toupper(*s2); // this is possibly naive for some languages...
    if (d) return d;
    s1++,s2++;
  }
  return *s2;
}
#endif
///

/// Check for the availability of strncasecmp and implement itif it does not exist.
#ifdef _WIN32
# define strncasecmp(a,b,n) _strnicmp((a),(b),(n))
#endif
#if !HAVE_STRNCASECMP
#include <ctype.h>
inline static int strncasecmp(const char *s1, const char *s2,size_t l) throw()
{
  int d;
  
  while(l && *s1) {
    d = toupper(*s1) - toupper(*s2); // this is possibly naive for some languages...
    if (d) return d;
    s1++,s2++,l--;
  }
  return *s2;
}
#endif
///

/// Check for availibility of the strchr function and provide a replacement if there is none.
#if !HAVE_STRCHR
inline static char *strchr(const char *s, int c) throw()
{
  while(*s) {
    if (*s == c) return const_cast<char *>(s);
    s++;
  }
  return 0;
}
#endif
///

/// Check for strerror function
#if !HAVE_STRERROR
// we cannot really do much about it as this is Os depdendent. Yuck.
#define strerror(c) "unknown error"
#endif
///

/// Check for strrchr function and provide a replacement.
#if !HAVE_STRRCHR
inline static char *strrchr(const char *s, int c) throw()
{
  const char *t = s + strlen(s);
  
  while(t > s) {
    t--;
    if (*t == c) return const_cast<char *>(t);
  }
  return 0;
}
#endif
///

///
#endif
