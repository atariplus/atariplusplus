/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: patch.cpp,v 1.9 2005-07-10 15:26:00 thor Exp $
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
void Patch::InstallPatchList(class AdrSpace *adr)
{   
  UBYTE esc         = 0;
  class Patch *ptch = this;
  // Install the complete patch list by traversing the patch queue.
  // Further assign the esc code slots here.
  do {
    if (ptch->NumPatches) {
      ptch->MinCode = esc;
      ptch->MaxCode = UBYTE(esc + ptch->NumPatches - 1);
    } else {
      ptch->MinCode = ptch->MaxCode = 0xff;
    }
    esc          +=       ptch->NumPatches;
    ptch->InstallPatch(adr,ptch->MinCode);
    ptch = ptch->NextOf();
  } while (ptch);
}
///

/// Patch::RunEmulatorTrap
// This is for the CPU emulator: Find a patch by an ESC code and
// dispatch it. Call this for the first patch in a list of
// patches. Returns true in case the patch could have been dispatched.
bool Patch::RunEmulatorTrap(class AdrSpace *adr,class CPU *cpu,UBYTE code)
{
  class Patch *ptch = this;
  do {
    if (ptch->NumPatches && code >= ptch->MinCode && code <= ptch->MaxCode) {
      ptch->RunPatch(adr,cpu,UBYTE(code - ptch->MinCode));
      return true;
    }
    // Try the next.
    ptch = ptch->NextOf();
  } while(ptch);
  //
  // Could not be dispatched.
  return false;
}
///
