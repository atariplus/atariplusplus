/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: types.hpp,v 1.17 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Thor's pecularities, my type definitions
 **********************************************************************************/

#ifndef TYPES_HPP
#define TYPES_HPP

/// NULL
#include "types.h"
#ifndef NULL
#if HAS__NULL_TYPE
#define NULL (__null)
#else
#define NULL (0)
#endif
#endif
///

/// Compiler workarounds
#ifdef HAS_LOCAL_TEMPLATES
#define T(super,type) type
#else
#define T(super,type) super::type
#endif
#ifdef HAS_MEMBER_INIT
#define INIT(a) = a
#else
#define INIT(a)
#endif

#ifdef HAS_CONST_CAST
#define TOCHAR(a) const_cast<char *>(a)
#define TOUBYTE(a) const_cast<UBYTE *>(a)
#else
#define TOCHAR(a) a
#define TOUBYTE(a) a
#endif
///

/// Basic types
#if SIZEOF_CHAR != 1
# error "No 8 bit type available"
#endif
typedef signed char         BYTE;    /* an 8 bit signed integer */
typedef unsigned char       UBYTE;   /* an 8 bit unsigned integer */

#if SIZEOF_SHORT == 2
typedef signed short int    WORD;    /* an 16 bit signed integer */
typedef unsigned short int  UWORD;   /* an 16 bit unsigned integer */
#elif SIZEOF_INT == 2
typedef signed int          WORD;    /* an 16 bit signed integer */
typedef unsigned int        UWORD;   /* an 16 bit unsigned integer */
#else
# error "No 16 bit type available"
#endif

#if SIZEOF_LONG  == 4
typedef signed long int     LONG;    /* an 32 bit signed integer */
typedef unsigned long int   ULONG;   /* an 32 bit unsigned integer */
#define LD "%ld"
#define LX "%lx"
#define LU "%lu"
#elif SIZEOF_INT == 4
typedef signed int          LONG;    /* an 32 bit signed integer */
typedef unsigned int        ULONG;   /* an 32 bit unsigned integer */
#define LD "%d"
#define LX "%x"
#define LU "%u"
#else
# error "No 32 bit integer type available"
#endif

// The following is not available on every compiler.
// They might be called differently on your machine, hence you might
// have to change these...
#if SIZEOF_LONG == 8
typedef signed long         QUAD;    /* an 64 bit signed long */
typedef unsigned long       UQUAD;   /* an 64 bit unsigned long */
# define HAVE_QUAD
#else
# if defined(SIZEOF___INT64)
#  if SIZEOF___INT64 == 8
typedef __int64             QUAD;
typedef unsigned __int64    UQUAD;
#   define HAVE_QUAD
#  endif
# endif
# ifndef HAVE_QUAD
#  if defined(SIZEOF_LONG_LONG)
#   if SIZEOF_LONG_LONG == 8
typedef signed long long    QUAD;    /* an 64 bit signed long */
typedef unsigned long long  UQUAD;   /* an 64 bit unsigned long */
#    define HAVE_QUAD
#   endif
#  endif
# endif
#endif
#ifndef HAVE_QUAD
# error "No 64 bit integer available"
#endif
///

/// Emulation specific types
// An address, as used by the emulator. We use here int 
// for speed reasons.
typedef int ADR;
///

/// Compiler __attributes__
// Checks whether the compiler has the __attribute__
// extension available. If so, we can use additional type 
// checks for some varar-dic methods.
#if HAS_ATTRIBUTES
#define PRINTF_STYLE __attribute__ (( format(printf,2,3) ))
#define LONG_PRINTF_STYLE(fmt,args) __attribute__ (( format(printf,fmt,args) ))
#else
#define PRINTF_STYLE
#define LONG_PRINTF_STYLE(fmt,args)
#endif
///

///
#endif


