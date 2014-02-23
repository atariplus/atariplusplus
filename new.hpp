/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: new.hpp,v 1.2 2013-03-16 15:08:52 thor Exp $
 **
 ** In this module: Customized memory handling functions
 **********************************************************************************/

#ifndef NEW_HPP
#define NEW_HPP

/// Includes
#include "types.h"
#include "string.hpp"
///

/// Replacement operator new, new[], delete and delete[]
void *operator new(size_t s);
void *operator new[] (size_t s);
void operator delete (void *a) throw();
void operator delete[] (void *a) throw();
///

///
#endif
