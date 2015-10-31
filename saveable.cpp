/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: saveable.cpp,v 1.5 2015/05/21 18:52:42 thor Exp $
 **
 ** This class defines the interface for reading and writing snapshots of 
 ** configurations.
 **********************************************************************************/

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "saveable.hpp"
#include "machine.hpp"
///

/// Saveable::Saveable
// Construct a saveable object
Saveable::Saveable(class Machine *mach,const char *name,int unit)
  : SaveName(name), SaveUnit(unit)
{
  //
  // Add the saveable to the head of saveables in the machine.
  mach->SaveableChain().AddHead(this);
}
///

/// Saveable::~Saveable
// Dispose a saveable and unlink it from the
// machine list to do so.
Saveable::~Saveable(void)
{
  Node<Saveable>::Remove();
}
///

