/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: new.hpp,v 1.4 2020/07/18 16:32:40 thor Exp $
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
#ifndef VALGRIND
void *operator new(size_t s);
void *operator new[] (size_t s);
#ifdef HAS_NOEXCEPT
void operator delete (void *a) noexcept;
void operator delete[] (void *a) noexcept;
void operator delete (void *a,size_t) noexcept;
void operator delete[] (void *a,size_t) noexcept;
#else
void operator delete (void *a) throw();
void operator delete[] (void *a) throw();
void operator delete (void *a,size_t) throw();
void operator delete[] (void *a,size_t) throw()
#endif
#endif
///

///
#endif
