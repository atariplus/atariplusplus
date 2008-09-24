/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: unistd.cpp,v 1.1 2002/11/12 22:45:09 thor Exp $
 **
 ** In this module: Os compatibility layer for (non-ANSI) file management.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions in case the host operating system
 ** does not implement them.
 **********************************************************************************/

/// Includes
#include "unistd.hpp"
///
