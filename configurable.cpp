/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: configurable.cpp,v 1.7 2015/05/21 18:52:37 thor Exp $
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
