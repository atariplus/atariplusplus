/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: patch.cpp,v 1.11 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Generic administration of patches
 **********************************************************************************/

/// Includes
#include "patch.hpp"
#include "adrspace.hpp"
#include "mmu.hpp"
///

/// Statics
#ifndef HAS_MEMBER_INIT
const int Patch::ESC_Code = 0x22; // The CPU OpCode for escaping (+RTS)
#endif
///

/// Patch::InsertESC
// Service: Install an ESC code into the ROM followed by an ESC identifier
void Patch::InsertESC(class AdrSpace *adr,ADR mem,UBYTE code)
{
  adr->PatchByte(mem  ,ESC_Code);
  adr->PatchByte(mem+1,code);
}
///

/// Patch::InstallPatchList
// This is for the maintainer of the patch list: Install all patches
// in a row. Call it from the first patch at hand.
void Patch::InstallPatchList(class Machine *mach,class AdrSpace *adr)
{   
  if (NumPatches) {
    MinCode = mach->AllocateEscape(NumPatches);
    MaxCode = UBYTE(MinCode + NumPatches - 1);
  } else {
    MinCode = MaxCode = 0xff;
  }
  InstallPatch(adr,MinCode);
}
///

/// Patch::RunEmulatorTrap
// This is for the CPU emulator: Find a patch by an ESC code and
// dispatch it. Call this for the first patch in a list of
// patches. Returns true in case the patch could have been dispatched.
bool Patch::RunEmulatorTrap(class AdrSpace *adr,class CPU *cpu,UBYTE code)
{
  if (NumPatches && code >= MinCode && code <= MaxCode) {
    RunPatch(adr,cpu,UBYTE(code - MinCode));
    return true;
  }
  //
  // Could not be dispatched.
  return false;
}
///
