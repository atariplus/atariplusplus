/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: memcontroller.hpp,v 1.8 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Interface definitions for memory controller.
 **********************************************************************************/

#ifndef MEMCONTROLLER_HPP
#define MEMCONTROLLER_HPP

/// Includes
#include "types.h"
///

/// Forwards
class Machine;
///

/// Class MemController
// This class is an abstract interface that provides one 
// additional method to initialize it. This 
// initializer is called after setup but before reset time
// and should be used to load the ROM, or to initialize the
// address space, or whatever.
class MemController {
  //
  // No members
  //
public:
  virtual ~MemController(void)
  {}
  //
  // Import this single method: Initialize us.
  virtual void Initialize(void) = 0;
};
///

///
#endif

