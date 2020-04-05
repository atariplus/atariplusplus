/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: patchprovider.cpp,v 1.12 2020/03/28 13:10:01 thor Exp $
 **
 ** In this module: Interface class that bundles patches into a group
 **********************************************************************************/

/// Includes
#include "patchprovider.hpp"
#include "patch.hpp"
#include "machine.hpp"
#include "adrspace.hpp"
#include "mmu.hpp"
///

/// PatchProvider::InstallPatchList
// Install all patches on the list
void PatchProvider::InstallPatchList(void)
{
  class AdrSpace *ram = Machine->MMU()->CPURAM();
  //
  InstallPatchList(ram);
}
///

/// PatchProvider::InstallPatchList
// Install all patches on the list in the given address space.
void PatchProvider::InstallPatchList(class AdrSpace *adr)
{
  class Patch *patch  = patchList.First();
  //
  // Now allocate the ESC codes and hack the patches in.
  while(patch) {
    patch->InstallPatchList(Machine,adr);
    patch = patch->NextOf();
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
  class Patch *p = patchList.First();

  while(p) {
    if (p->RunEmulatorTrap(adr,cpu,code))
      return true;
    p = p->NextOf();
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

