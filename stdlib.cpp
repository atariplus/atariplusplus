/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: stdlib.cpp,v 1.7 2020/07/18 16:32:40 thor Exp $
 **
 ** In this module: Os compatibility layer
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them. This wrapper is for functions within stdlib
 **********************************************************************************/

/// Includes
#include "stdlib.hpp"
#include <ctype.h>
///

/// Check for strtol function and provide a replacement if we have none
#if HAVE_STRTOL
#else
// Do not trust errno, limits and ctype. We know nothing about this system. Yuck.
// Perform a minimal installation that is good enough for our purposes.
#ifdef HAS_NOEXCEPT
LONG int strtol(const char *nptr, char **endptr, int base) noexcept
#else
LONG int strtol(const char *nptr, char **endptr, int base) throw()
#endif
{
  LONG n        = 0;
  bool negative = false;
  bool invalid  = false;
  // skip leading spaces
  while(*nptr == ' ' || *nptr == '\t' || *nptr == '\n')
    nptr++;
  // skip leading signs, set negative flag if appropriate
  if (*nptr == '+') {
    nptr++;
  } else if (*nptr == '-') {
    nptr++;
    negative = true;
  }
  if (base == 0 || base == 16) {
    // hex basis. Check whether we have a leading "0x". If so, skip this and set the base to 16.
    if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
      nptr += 2;
      base  = 16;
    }
  }
  if (base == 0 || base == 8) {
    // octal basis. Check for a leading '0' and skip it if so.
    if (nptr[0] == '0') {
      nptr++;
      base = 8;
    }
  }
  if (base == 0) {
    // otherwise assume decimal
    base = 10;
  }
  // Now perform the conversion
  do {
    unsigned char c = toupper(*nptr);
    // Check whether c is valid. If so, bring it into range.
    if (c >= '0' && c <= '9') {
      c -= '0';
    } else if (c >= 'A' && c <= 'Z') {
      c -= 'A' - 10;
    } else if (c >= 'a' && c <= 'z') {
      c -= 'a' - 10;
    } else break;
    if (c >= base) break;
    // Check whether we would wrap around. If so, we are invalid. Problem case: LONG_MIN!
    // We do not handle this case correctly here.
    if (!invalid) {
      LONG m = n * base + c;
      if (m < 0) {
	invalid = true;
      } else {
	n = m;
      }
    }
    // This character is valid, continue with the next.
    nptr++;
  } while(true);
  // Check whether we have a sign. If so, set the result.    
  if (invalid) {
    n = ((LONG)(~0)) >> 1;            // this is the largest LONG on byte sized binary machines.
  }
  if (negative) {
    if (invalid) {
      n++;                            // this is the smallest LONG on byte sized binary machines. Sigh.
    } else {
      n = -n;
    }
  }
  if (endptr)
    *endptr = const_cast<char *>(nptr);

  return n;
}
#endif
///
