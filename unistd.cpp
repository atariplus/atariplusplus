/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: unistd.cpp,v 1.3 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Os compatibility layer for (non-ANSI) file management.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them.
 **********************************************************************************/

/// Includes
#include "unistd.hpp"
///
