/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: ramextension.cpp,v 1.2 2013-03-16 15:08:52 thor Exp $
 **
 ** In this module: Base class for all ram extension classes
 **********************************************************************************/

/// Includes
#include "ramextension.hpp"
#include "machine.hpp"
#include "mmu.hpp"
///

/// RamExtension::RamExtension
// Construct a RAM extension class by constructing the
// "Saveable" underneath.
RamExtension::RamExtension(class Machine *mach,const char *name)
  : Saveable(mach,name), MMU(mach->MMU())
{
}
///

/// RamExtension::~RamExtension
// Dispose the RamExtension again. This *does not* unlink us from
// any list whatsoever. Ram Extensions are "passive" clients of
// the MMU.
RamExtension::~RamExtension(void)
{
}
///
