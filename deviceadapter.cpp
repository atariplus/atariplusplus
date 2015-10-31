/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: deviceadapter.cpp,v 1.5 2015/05/21 18:52:38 thor Exp $
 **
 ** In this module: Device maintainment
 **********************************************************************************/

/// Includes
#include "deviceadapter.hpp"
#include "adrspace.hpp"
#include "mmu.hpp"
#include "cpu.hpp"
///

/// DeviceAdapter::HInit
// This is the table the Os should use to initialize HATABS.
// Well, I hope...
const UBYTE DeviceAdapter::HInit[] = {
  'P',0x30,0xe4,
  'C',0x40,0xe4,
  'E',0x00,0xe4,
  'S',0x10,0xe4,
  'K',0x20,0xe4
};
///

/// DeviceAdapter::CIOInit
// This is the call to CIO init to be looked for in the ROM
// area. It is patched over to replace it by our custom
// installation routine.
const UBYTE DeviceAdapter::CIOInit[] = {
  0xe4,0x20,0x6e,0xe4,0x20
};
///

/// DeviceAdapter::FindString
// Find a specified string in an address space given a range. 
// Returns either 0 if not found, or the address.
// This is used to locate the Hatabs init table
ADR DeviceAdapter::FindString(class AdrSpace *adr,ADR from,ADR to,const UBYTE needle[],size_t size)
{
  ADR where;

  // The string we search for cannot remain in the last
  // bytes unless there is enough room to account for it.
  to = ADR(to - size + 1);
  for(where = from;where <= to;where++) {
    const UBYTE *scan;
    ADR   scanpos;
    size_t sz;
    
    for(scanpos = where,scan = needle,sz = size;sz;scanpos++,scan++,sz--) {
      if (adr->ReadByte(scanpos) != *scan)
	break;
    }
    // If sz is now zero, then we've found the position
    if (sz == 0)
      return where;
  }
  return 0;
}
///

/// DeviceAdapter::FindOsString
// Find a binary string in the OS rom, return its address (if any).
ADR DeviceAdapter::FindOsString(class AdrSpace *adr,const UBYTE needle[],size_t size)
{ 
  // Depending on the Os type, we might have to look in
  // various areas
  switch(Machine->OsROM()->RomType()) {
  case OsROM::Os_RomA:
    if (OsAHatabs == 0)
      OsAHatabs = FindString(adr,0xe400,0xffff,needle,size);
    return OsAHatabs;
  case OsROM::Os_RomB:
    if (OsBHatabs == 0)
      OsBHatabs = FindString(adr,0xe400,0xffff,needle,size);
    return OsBHatabs;
  case OsROM::Os_RomXL:
    if (OsXLHatabs == 0) {
      OsXLHatabs = FindString(adr,0xe400,0xffff,needle,size);
      if (OsXLHatabs == 0)
	OsXLHatabs = FindString(adr,0xc000,0xcbff,needle,size);
    }
    return OsXLHatabs;
  case OsROM::Os_Rom1200:
    if (Os1200Hatabs == 0) {
      Os1200Hatabs = FindString(adr,0xe400,0xffff,needle,size);
      if (Os1200Hatabs == 0)
	Os1200Hatabs = FindString(adr,0xc000,0xcbff,needle,size);
    }
    return Os1200Hatabs;
  case OsROM::Os_Builtin:
    if (OsBuiltinHatabs == 0) {
      OsBuiltinHatabs = FindString(adr,0xe400,0xffff,needle,size);
      if (OsBuiltinHatabs == 0)
	OsBuiltinHatabs = FindString(adr,0xc000,0xcbff,needle,size);
    }
    return OsBuiltinHatabs;
  default:
    // shut up the compiler
    Throw(NotImplemented,"Machine::FindOsString","unknown Os revision");
    break;
  }
  return 0;
}
///

/// DeviceAdapter::RunPatch
// Implemenation of the patch interface:
// Run the patch that installs additional devices into HATABS
void DeviceAdapter::RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE )
{
  ADR handler = 0xd700;
  ADR hatabs  = 0x0329;
  ADR pc;
  UBYTE letter,stck;
  // This is run "in place" of the CIO init routine. We first use our chance
  // here and extend the HATABS entry list by all devices found in the
  // extended handler area from 0xd700 to 0xd7ff.
  while(handler < 0xd800 && (letter = adr->ReadByte(handler + 0x0f))) {
    // Ok, another device to patch in. Check for available space in HATABS,
    // then link in.
    while(hatabs < 0x33d && adr->ReadByte(hatabs)) {
      hatabs += 3;
    }
    if (hatabs >= 0x33d) {
      // Run out of data. Yuck.
      Throw(OutOfRange,"DeviceAdapter::InstallDevice","out of HATABS space for new devices");
    }
    // Now provide the device letter.
    adr->WriteByte(hatabs,letter);
    // And the entry point for the device.
    adr->WriteByte(hatabs + 1,UBYTE(handler & 0xff));
    adr->WriteByte(hatabs + 2,UBYTE(handler >> 8));
    hatabs  += 3;
    handler += 0x20;
  }
  // Finally, manipulate the stack to make a jump to the CIO init code.
  stck      = cpu->S();
  pc        = cpu->PC();
  adr->WriteByte(0x100 + stck--,UBYTE((pc) >> 8));
  adr->WriteByte(0x100 + stck--,UBYTE((pc) & 0xff));
  adr->WriteByte(0x100 + stck--,UBYTE((0xe46e - 1) >> 8));
  adr->WriteByte(0x100 + stck--,UBYTE((0xe46e - 1) & 0xff));
  cpu->S() = stck;
}
///

/// DeviceAdapter::InstallPatch
// Find the call of CIOInit in the OsROM, then install us there:
// This is not done here, but rather as soon as the first device needing
// this mechanism has to be patched in.
void DeviceAdapter::InstallPatch(class AdrSpace *,UBYTE code)
{
  PatchCode = code;
}
///

/// DeviceAdapter::ReplaceDevice
// Find the ROM entry point for the device given its device letter,
// and replace it. Returns true in case this was successfull, false
// otherwise (as the device must then be installed into the handler ROM)
bool DeviceAdapter::ReplaceDevice(class AdrSpace *adr,UBYTE code,UBYTE deviceslot,UBYTE deviceletter,ADR *old)
{
  int i;
  ADR hatabs,table;
  UBYTE letter;
  //
  // Ask for the Hatabs base: Depends on the ROM version
  hatabs = FindOsString(adr,HInit,sizeof(HInit));
  if (hatabs == 0) {
    Throw(InvalidParameter,"DeviceAdapter::InstallPatch","unable to find the location of HATABS init");
  }
  //
  // We have exactly five entries in the ROM hatabs 
  // (hard-coded value, bah!): S:,E:,K:,C:,P:
  for(i=0;i<5;i++) {
    letter = UBYTE(adr->ReadByte(hatabs));
    table  = ADR(adr->ReadWord(hatabs+1));
    if (letter == deviceslot) {
      int i;
      ADR vecs[6],dest = NextPatchEntry;
      // Check whether there is enough room for the patched code in the
      // reserved page.
      if (dest + 0x20 >= 0xd800)
	Throw(OutOfRange,"DeviceAdapter::InstallDevice","out of ROM space for device patches");
      // We have found our device slot, now patch it in.
      // table is the address with the entry points for this device.
      // Note that CIO use an RTS to jump to this table, hence the
      // need to add here a one.
      adr->PatchByte(hatabs,deviceletter);
      // Now read all vectors.
      for(i=0;i<6;i++) {
	vecs[i] = dest + (i << 1); // two bytes per entry.
	old[i]  = adr->ReadWord(table + (i << 1)) + 1;
	adr->PatchByte(table + (i<<1) + 0,UBYTE( vecs[i] - 1));
	adr->PatchByte(table + (i<<1) + 1,UBYTE((vecs[i] - 1)>>8));
	InsertESC(adr,vecs[i],UBYTE(code + i));
      }
      adr->PatchByte(table + 0x0c,0x60);                       // patch the initcode over
      dest += 0xd;
      while(dest < 0xd800) {
	adr->PatchByte(dest,0x00);
	dest++;
      }
      NextPatchEntry += 0x10;      // That's it!
      return true;
    } 
    hatabs += 3;
  }
  //
  // Not found. Bail out.
  return false;
}
///

/// DeviceAdapter::InstallDevice
// To be called by a device to get registered.
void DeviceAdapter::InstallDevice(class AdrSpace *adr,UBYTE patchcode,UBYTE slot,UBYTE letter,ADR *old)
{
  // 
  // First check whether we can replace a ROM device of the given slot.
  if (ReplaceDevice(adr,patchcode,slot,letter,old)) {
    return;
  } else {
    int i;
    ADR dest;
    //
    if (PatchedHook == false) {
      ADR cioinit;
      // CIO init is not yet patched over. Do now.
      cioinit = FindOsString(adr,CIOInit,sizeof(CIOInit));
      if (cioinit == 0) {
	Throw(InvalidParameter,"DeviceAdapter::InstallDevice","unable to find the location of CIO init");
      }
      InsertESC(adr,cioinit + 1,PatchCode);
      adr->PatchByte(cioinit + 3,0xea);
      PatchedHook = true;
    }
    // Check whether there's enough room for the entry.
    if (NextPatchEntry >= 0xd800) {
      Throw(OutOfRange,"DeviceAdapter::InstallDevice","out of ROM space for new devices");
    }
    dest = NextPatchEntry;
    for(i = 0;i < 6;i++) { 
      // Build up the vectors for open, close, get, put, status, special
      adr->PatchByte(dest + (i << 1) + 0,(dest + 0x10 + (i << 1) - 1) & 0xff); // LO
      adr->PatchByte(dest + (i << 1) + 1,(dest + 0x10 + (i << 1) - 1) >> 8);   // HI
      InsertESC(adr,dest + 0x10 + (i << 1),UBYTE(patchcode + i));              // target patch address
      old[i] = 0x0000; // not available.
    }
    // Insert dummy init address.
    adr->PatchByte(dest + 0x0c,0x60);   // A single RTS does it.
    adr->PatchByte(dest + 0x0d,0x00);
    adr->PatchByte(dest + 0x0e,0x00);
    adr->PatchByte(dest + 0x0f,letter); // provide the letter of the device under which this should appear
    //
    // Zero the remaining data out so we know where to stop at the init point.
    dest += 0x1c;
    while(dest < 0xd800) {
      adr->PatchByte(dest,0x00);
      dest++;
    }
    NextPatchEntry += 0x20;
  }
}
///

/// DeviceAdapter::DeviceAdapter
DeviceAdapter::DeviceAdapter(class Machine *mach,class PatchProvider *p)
  : Patch(mach,p,1), Machine(mach), PatchedHook(false)
{
  // We need exactly one patch vector here, namly on top of CIOINIT
  OsAHatabs       = 0;
  OsBHatabs       = 0;
  OsXLHatabs      = 0;
  Os1200Hatabs    = 0;
  OsBuiltinHatabs = 0;
  NextPatchEntry  = 0xd700; // Target address for patched-in HATABS entries.

}
///
