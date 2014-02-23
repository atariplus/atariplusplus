/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: hbiaction.cpp,v 1.2 2013-03-16 15:08:52 thor Exp $
 **
 ** In this module: Interface for frequent operations that have to happen
 **                 each horizontal blank periodically.
 **********************************************************************************/

/// Includes
#include "machine.hpp"
#include "hbiaction.hpp"
///

/// HBIAction::HBIAction
HBIAction::HBIAction(class Machine *mach)
{
  // First constructed classes must go first.
  mach->HBIChain().AddTail(this);
}
///

/// HBIAction::~HBIAction
HBIAction::~HBIAction(void)
{
  Remove();
}
///
