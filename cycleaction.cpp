/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cycleaction.cpp,v 1.2 2011-05-01 09:23:58 thor Exp $
 **
 ** In this module: Interface for frequent operations that have to happen
 **                 on each CPU cycle.
 **********************************************************************************/

/// Includes
#include "machine.hpp"
#include "cycleaction.hpp"
///

/// CycleAction::CycleAction
CycleAction::CycleAction(class Machine *)
{
  // This is not an auto-add class.
}
///

/// CycleAction::~CycleAction
CycleAction::~CycleAction(void)
{
  // This is not an auto-add class, thus do not remove the class.
}
///
