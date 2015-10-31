/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: unistd.hpp,v 1.11 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Os compatibility layer for (non-ANSI) file management.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them.
 **********************************************************************************/

#ifndef UNISTD_HPP
#define UNISTD_HPP

/// Includes
#include "types.h"

#if HAVE_UNIX_H
# include <unix.h>
#endif

// Some unix derivates require the bzero function for select which
// is then in bstrings.h or bstring.h. We try to include them
// here.
#if HAVE_BSTRINGS_H
#include <bstrings.h>
#elif HAVE_BSTRING_H
#include <bstring.h>
#endif
///

/// Provide some Os layer functionality and dummies
// If your Os provides suitable custom functions that are
// not handled by these function sets, please add replacement
// functions. The dummies currently do not perform any activity,
// hence some functions just won't work.
// This is for joystick input, mainly. We can do without... I hope.
#if HAVE_UNISTD_H
# include <unistd.h>
// Check whether we don't have usleep but we do have select (IRIX).
// If so, emulate a usleep by means of select
# if !HAVE_USLEEP
#  if HAVE_SELECT
#   include "time.hpp"
static int usleep(ULONG usec)
{
  struct timeval tv;

  tv.tv_sec  = 0;
  tv.tv_usec = usec;

  return select(0,NULL,NULL,NULL,&tv);
}
#  else // no usleep and no select. Yikes!
#   define usleep(x) 0
#  endif // have no usleep
# endif
#else   // have unistd.h, else part
//
// Here no unistd.h is available. This means no audio, no joystick. Just provide dummies.
//
// Sometimes these functions are part of stdio.h. Try this
#include <stdio.h>
//
//
#ifndef HAVE_READ
#define read(fd,buf,count) (-1)
#endif
#ifndef HAVE_WRITE
#define write(fd,buf,count) (-1)
#endif
//
#ifndef HAVE_OPEN
// open exists twice and hence has to be "implemented" by inline functions
inline static int open(const char *, int)
{
  return -1;
}

inline static int open(const char*, int, int)
{
  return -1;
}
#endif
#ifndef HAVE_CREAT
#define creat(pathname,mode) (-1)
#endif
#ifndef HAVE_CLOSE
#define close(fd) (0)
#endif

#ifndef HAVE_IOCTL
// Provide a dummy ioctl. This is mainly for joystick/sound control.
// If this doesn't work, then we don't have sound. Tuff luck.
static int ioctl(int,int, ...)
{
  return -1;
}
#endif

#ifndef HAVE_UNLINK
// Provide a dummy unlink. This is only for cosmetical reasons in the H: handler
// and need not to function.
static int unlink(const char *)
{
  return 0;
}
#endif

#ifndef HAVE_USLEEP
// Provide a dummy usleep. Hmm.
#define usleep(micros) (0)
#endif
#endif
///

///
#endif
