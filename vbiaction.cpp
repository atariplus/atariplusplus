/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: vbiaction.cpp,v 1.8 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Interface for frequent operations that have to happen
 **                 each VBI periodically.
 **********************************************************************************/

/// Includes
#include "machine.hpp"
#include "vbiaction.hpp"
///

/// VBIAction::VBIAction
VBIAction::VBIAction(class Machine *mach)
{
  mach->VBIChain().AddTail(this); // because sound must go last!
}
///

/// VBIAction::~VBIAction
VBIAction::~VBIAction(void)
{
  Remove();
}
///
