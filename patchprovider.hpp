/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: patchprovider.hpp,v 1.4 2003/02/01 20:32:02 thor Exp $
 **
 ** In this module: Interface class that bundles patches into a group
 **********************************************************************************/

#ifndef PATCHPROVIDER_HPP
#define PATCHPROVIDER_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "machine.hpp"
///

/// Forwards
class AdrSpace;
class CPU;
class Patch;
///

/// Class PatchProvider
// This class describes an interface that combines several patches that
// go into the same page/address space and that are removed and installed
// in common. Patches into the desired address range have to be provided
// by a single provider only, and no other provider object may patch the
// same address range.
class PatchProvider : public Node<class PatchProvider> {
  //
  class Machine       *Machine;
  //
protected:
  // A list of patches, for linkage
  List<Patch>          patchList;
  //
  // PatchProviders itself are not constructable. You must derive a real object
  // from them. You need to provide the machine to allocate the patch
  // from.
  PatchProvider(class Machine *mach)
    : Machine(mach)
  { 
    mach->PatchList().AddHead(this);
  }
  //
  ~PatchProvider(void)
  {
    // Kill all patches here
    DisposePatches();
  }
  //
  // Reset all patches. This is called on a warmstart or coldstart.
  void Reset(void);
  //
public:
  // This is why we are here: Run all patches we and other providers
  // we are linked to know of. This must be called with the head of
  // the patch provider list.
  bool RunEmulatorTrap(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  // 
  // Install all patches on the list
  void InstallPatchList(void);
  //
  // Get rid of all installed patches now; dispose them all.
  void DisposePatches(void);
  //
  // Return the patch list to install a patch into
  List<Patch> &PatchList(void)
  {
    return patchList;
  }
  //
};
///

///
#endif
