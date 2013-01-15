/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: patchprovider.cpp,v 1.7 2003-04-03 21:39:52 thor Exp $
 **
 ** In this module: Interface class that bundles patches into a group
 **********************************************************************************/

/// Includes
#include "patchprovider.hpp"
#include "machine.hpp"
#include "adrspace.hpp"
#include "mmu.hpp"
///

/// PatchProvider::InstallPatchList
// Install all patches on the list
void PatchProvider::InstallPatchList(void)
{
  // Now allocate the ESC codes and hack the patches in.
  if (patchList.First()) {
    class AdrSpace *ram = Machine->MMU()->CPURAM();
    patchList.First()->InstallPatchList(ram);
  }
}
///

/// PatchProvider::DisposePatches
// Get rid of all installed patches now.
void PatchProvider::DisposePatches(void)
{
  class Patch *next;

  while((next = patchList.First())) {
    // This also unlinks us from the list.
    delete next;
  }
}
///

/// PatchProvider::RunEmulatorTrap
// This is why we are here: Run all patches we and other providers
// we are linked to know of. This must be called with the head of
// the patch provider list.
bool PatchProvider::RunEmulatorTrap(class AdrSpace *adr,class CPU *cpu,UBYTE code)
{
  class PatchProvider *prov = this;
  while(prov) {
    if (prov->patchList.First()) {
      if (prov->patchList.First()->RunEmulatorTrap(adr,cpu,code))
	return true;
    }
    prov = prov->NextOf();
  }
  return false;
} 
///

/// PatchProvider::Reset
// Reset all patches. This is called on a warmstart or coldstart.
void PatchProvider::Reset(void)
{
  class Patch *p;

  for(p=patchList.First();p;p=p->NextOf())
    p->Reset();
}
///

