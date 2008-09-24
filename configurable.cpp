/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: configurable.cpp,v 1.5 2003/02/01 20:32:02 thor Exp $
 **
 ** In this module: Configurable module(s)
 **********************************************************************************/

/// Includes
#include "list.hpp"
#include "configurable.hpp"
#include "machine.hpp"
///

/// Configurable::Configurable
Configurable::Configurable(class Machine *mach)
{
  mach->ConfigChain().AddTail(this);
}
///

/// Configurable::~Configurable
Configurable::~Configurable(void)
{
  Remove();
}
///
