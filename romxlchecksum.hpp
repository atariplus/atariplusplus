/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: romxlchecksum.hpp,v 1.6 2003-04-12 12:16:22 thor Exp $
 **
 ** In this module: Patch the ROM XL checksum to its proper value
 **********************************************************************************/

#ifndef ROMXLCHECKSUM_HPP
#define ROMXLCHECKSUM_HPP

/// Includes
#include "patch.hpp"
///

/// Forwards
class PatchProvider;
///

/// Class RomXLChecksum
// This patch corrects the ROM checksum such that the selftest performs
// fine.
class RomXLChecksum : public Patch {
  //
  class Machine *Machine;
  //
  // Implementations of the Patch interface:
  // This entry is called whenever a new ROM is loaded. It is required
  // to install the patch into the image.
  virtual void InstallPatch(class AdrSpace *adr,UBYTE code);
  //
  // This entry is called by the CPU emulator to run the patch at hand
  // whenever an ESC (HLT, JAM) code is detected.
  virtual void RunPatch(class AdrSpace *,class CPU *,UBYTE)
  { 
    // As we don't emulate anything, there is no code here either.
  }
  //
  // Run a word checksum over a range of the Os ROM, lo inclusive,
  // hi exclusive.
  UWORD CheckSum(ADR lo,ADR hi);
  //
public:
  RomXLChecksum(class Machine *mach,class PatchProvider *p);
  virtual ~RomXLChecksum(void)
  { 
  }
  //
};
///

///
#endif
