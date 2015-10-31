/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: patch.hpp,v 1.18 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Generic administration of patches
 **********************************************************************************/

#ifndef PATCH_HPP
#define PATCH_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "patchprovider.hpp"
///

/// Forwards
class AdrSpace;
class CPU;
///

/// Class Patch
// This patch describes ROM patches that are installed into the ROM Image
// to simplify the life of the emulator. To be precise, this rather
// describes a range of ESC codes, not just a single.
class Patch : public Node<class Patch> {
  //
  // The ESCcape code range for the patch. This identifies the patch uniquely
  UBYTE        MinCode,MaxCode,NumPatches;
  //
protected: 
  static const int ESC_Code INIT(0x22); // The CPU OpCode for escaping (+RTS)
  //
  // Patches itself are not constructable. You must derive a real object
  // from them. You need to provide the machine to allocate the patch
  // from.
  // A free slot for the ESC code is allocated automagically.
  Patch(class Machine *,class PatchProvider *plist,UBYTE numpatches)
    : NumPatches(numpatches)
  {
    // Must add patches in this order since the XL checksum patch
    // cleans up the checksum here.
    plist->PatchList().AddTail(this);
  }
  //
  // Service: Install an ESC code into the ROM followed by an ESC identifier
  void InsertESC(class AdrSpace *adr,ADR mem,UBYTE code);
  //
  // This entry is called whenever a new ROM is loaded. It is required
  // to install the patch into the image.
  virtual void InstallPatch(class AdrSpace *adr,UBYTE code) = 0;
  //
  // This entry is called by the CPU emulator to run the patch at hand
  // whenever an ESC (HLT, JAM) code is detected.
  virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code) = 0;  
  //
public:  
  //
  virtual ~Patch(void)
  {
    // Patches add themselves, so they should remove themsevles
    Remove();
  }
  // This is for the maintainer of the patch list: Install all patches
  // in a row. Call it from the first patch at hand.
  void InstallPatchList(class Machine *mach,class AdrSpace *adr);
  //
  // This is for the CPU emulator: Find a patch by an ESC code and
  // dispatch it. Call this for the first patch in a list of
  // patches. Returns true in case the patch could have been dispatched.
  bool RunEmulatorTrap(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  //
  // Reset this patch. This method can be overloaded if required.
  virtual void Reset(void)
  { } // Do nothing by default
  //
};
///

///
#endif
