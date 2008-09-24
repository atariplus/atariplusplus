/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: siopatch.cpp,v 1.6 2003/03/25 15:59:35 thor Exp $
 **
 ** In this module: SIOPatch for advanced speed communication
 **********************************************************************************/

/// Includes
#include "siopatch.hpp"
#include "sio.hpp"
#include "cpu.hpp"
#include "adrspace.hpp"
///

/// SIOPatch::SIOPatch
// Constructor. Needs to construct the Patch as well.
SIOPatch::SIOPatch(class Machine *mach,class PatchProvider *p,class SIO *siointerf)
  : Patch(mach,p,1), sio(siointerf)
{
}
///

/// SIOPatch::InstallPatch
// This entry is called whenever a new ROM is loaded. It is required
// to install the patch into the image.
void SIOPatch::InstallPatch(class AdrSpace *adr,UBYTE code)
{
  InsertESC(adr,0xe459,code);
}
///

/// SIOPatch::RunPatch
// This entry is called by the CPU emulator to run the patch at hand
// whenever an ESC (HLT, JAM) code is detected.
void SIOPatch::RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE)
{
  UBYTE device = UBYTE(adr->ReadByte(0x300));
  UBYTE unit   = UBYTE(adr->ReadByte(0x301));
  UBYTE cmd    = UBYTE(adr->ReadByte(0x302));
  ADR   mem    = ADR(adr->ReadWord(0x304));
  UBYTE timout = UBYTE(adr->ReadByte(0x306));
  UWORD size   = UWORD(adr->ReadWord(0x308));
  UWORD aux    = UWORD(adr->ReadWord(0x30a));
  UBYTE result;
  
  // Bypass the serial overhead for the SIO patch and issues the
  // command directly. It returns a status
  // indicator similar to the ROM SIO call.
  result = sio->RunSIOCommand(device,unit,cmd,mem,size,aux,timout);

  // Now install the result code of the above command.
  adr->WriteByte(0x303,result);
  cpu->Y() = result;
  if (result >= 0x80) {
    cpu->P() |=  CPU::N_Mask;
  } else {
    cpu->P() &= ~CPU::N_Mask;
  }
}
///

