/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: chip.cpp,v 1.5 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Definition of a generic chip
 **********************************************************************************/

/// Includes
#include "chip.hpp"
#include "machine.hpp"
///

/// Chip::Chip
// Constructor
Chip::Chip(class Machine *mach,const char *n)
  : Configurable(mach), machine(mach), name(n)
{ 
  mach->ChipChain().AddHead(this);
}
///

/// Chip::~Chip
// Release the chip and unlink it from the chain of chips
Chip::~Chip(void)
{ 
  Node<Chip>::Remove(); // the configurable removes itself
}
///
