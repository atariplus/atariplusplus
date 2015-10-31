/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: stdlib.hpp,v 1.6 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Os compatibility layer
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them. This wrapper is for functions within stdlib
 **********************************************************************************/

#ifndef STDLIB_HPP
#define STDLIB_HPP

/// Includes
#include "types.h"

#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
///

/// Check for strtol function and provide a replacement if we have none
#if HAVE_STRTOL
#else
LONG int strtol(const char *nptr, char **endptr, int base) throw();
#endif
///

#endif
