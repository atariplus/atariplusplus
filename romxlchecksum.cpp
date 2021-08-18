/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: romxlchecksum.cpp,v 1.10 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Patch the ROM XL checksum to its proper value
 **********************************************************************************/

/// Includes
#include "types.hpp"
#include "romxlchecksum.hpp"
#include "page.hpp"
#include "osrom.hpp"
#include "rompage.hpp"
#include "adrspace.hpp"
///

/// RomXLChecksum::RomXLChecksum
// The constructor
RomXLChecksum::RomXLChecksum(class Machine *mach,class PatchProvider *p)
  : Patch(mach,p,0), Machine(mach) // reserve zero slots here.
{ }
///

/// RomXLChecksum::CheckSum
// Run a word checksum over a range of the Os ROM, lo inclusive,
// hi exclusive.
UWORD RomXLChecksum::CheckSum(ADR lo,ADR hi)
{
  class RomPage *rom = Machine->OsROM()->OsPages();
  UWORD sum = 0;

  // Offset the low and hi such that we get the right address within the
  // ROM. Note that the selftest has its origin at d000-d800 and gets
  // mirrored into 5000-5800
  if (lo >= 0x5000 && hi <= 0x5800) {
    lo += 0x8000;
    hi += 0x8000;
  }
  // Now get the base into the XL rom. This is at 0xc000.
  lo -= 0xc000;
  hi -= 0xc000;

  do {
    sum += UWORD(rom[lo >> Page::Page_Shift].ReadByte(lo));
    lo++;
  } while(lo<hi);
  
  return sum;
}
///

/// RomXLChecksum::InstallPatch
// This entry is called whenever a new ROM is loaded. It is required
// to install the patch into the image.
void RomXLChecksum::InstallPatch(class AdrSpace *adr,UBYTE)
{
  UWORD sum;
  //
  // Fix up the ROM checksums.
  //
  sum  = CheckSum(0xc002,0xd000);
  if (Machine->OsROM()->RomType() != OsROM::Os_Rom1200) {
    // Due to an Os bug, the 1200XL does not sum this
    // part of the ROM
    sum += CheckSum(0x5000,0x5800);
  }
  sum += CheckSum(0xd800,0xe000);
  adr->PatchByte(0xc000,UBYTE(sum & 0xff));
  adr->PatchByte(0xc001,UBYTE(sum >> 8));
  //
  sum  = CheckSum(0xe000,0xfff8);
  sum += CheckSum(0xfffa,0x10000);
  adr->PatchByte(0xfff8,UBYTE(sum & 0xff));
  adr->PatchByte(0xfff9,UBYTE(sum >> 8));
}
///
