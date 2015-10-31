/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: siopatch.hpp,v 1.6 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: SIOPatch for advanced speed communication
 **********************************************************************************/

#ifndef SIOPATCH_HPP
#define SIOPATCH_HPP

/// Includes
#include "patch.hpp"
///

/// Forwards
class SIO;
class PatchProvider;
///

/// Class SIOPatch
// This patch is installed on top of the Os SIO routine to
// speedup communications with serial devices.
class SIOPatch : public Patch {
  //
  //
  // Pointer to the SIO device we need for communications.
  class SIO  *sio;
  //
  // Implementations of the Patch interface:
  // This entry is called whenever a new ROM is loaded. It is required
  // to install the patch into the image.
  virtual void InstallPatch(class AdrSpace *adr,UBYTE code);
  //
  // This entry is called by the CPU emulator to run the patch at hand
  // whenever an ESC (HLT, JAM) code is detected.
  virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  
public:
  SIOPatch(class Machine *mach,class PatchProvider *pp,class SIO *sio);
  virtual ~SIOPatch(void)
  { }
  //
};
///

///
#endif
