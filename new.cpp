/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: new.cpp,v 1.4 2020/03/28 13:10:01 thor Exp $
 **
 ** In this module: Customized memory handling functions
 **********************************************************************************/

/// Includes
#include "types.h"
#include "string.hpp"
#include "new.hpp"
#include "exceptions.hpp"
// Check for malloc. 
// If we do not have a malloc, we cannot work at all.
#if HAVE_MALLOC
#include "stdlib.hpp"
#else
#error "the system does not provide malloc"
#endif
///

/// Replacement operator new, new[], delete and delete[]
void *operator new(size_t s)
{
#if 1
  void *mem = malloc(s);
#else
  void *mem = calloc(s,1);
#endif

  if (mem == 0)
    Throw(NoMem,"operator new","out of memory");

  return mem;
}

void *operator new[] (size_t s)
{
#if 1
  void *mem = malloc(s);
#else
  void *mem = calloc(s,1);
#endif
  
  if (mem == 0)
    Throw(NoMem,"operator new[]","out of memory");

  return mem;
}

void operator delete (void *a) throw()
{
  free(a);
}

void operator delete (void *a,size_t) throw()
{
  free(a);
}

void operator delete[] (void *a) throw()
{
  free(a);
}

void operator delete[] (void *a,size_t) throw()
{
  free(a);
}
///
