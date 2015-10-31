/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: time.hpp,v 1.6 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Os abstraction for timing interfaces
 **********************************************************************************/

#ifndef TIME_HPP
#define TIME_HPP

/// Includes
#include "types.h"


#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# if HAVE_TIME_H
#  include <time.h>
# endif
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  ifdef _WIN32
#  include <time.h>
#  elif HAVE_TIME_H
#   include <time.h>
#  endif
# endif
#endif
//
// BSD seems to have a problem if we include it first.
// time_t is declared in time.h.
// Mac Metrowerks seems to require this here.
#if HAVE_UTIME_H && HAS_STRUCT_TIMEVAL
# include <utime.h>
#endif
///

///
#endif
