/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xeextension.cpp,v 1.13 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: This RAM extension implements the 130XE 64K of
 ** additional RAM.
 **********************************************************************************/

/// Includes
#include "xeextension.hpp"
#include "machine.hpp"
#include "monitor.hpp"
#include "adrspace.hpp"
#include "argparser.hpp"
#include "snapshot.hpp"
#include "mmu.hpp"
#include "stdio.hpp"
#include "new.hpp"
///

/// XEExtension::BankBits
// The array that defines the bits of PIA port B that give
// the bank #.
const UBYTE XEExtension::BankBits[8] = { 2,3,6,7,1,5,0,4 };
// Default values that are passed on to PIA in case we capture
// one of the bits is one.
///

/// XEExtension::XEExtension
// Construct the extension. We only have to initialize
// the members and have to set the name of the game.
XEExtension::XEExtension(class Machine *mach)
  : RamExtension(mach,"130XERamBanks"),
    RAM(new RamPage[256]),
    CPUBank(0), AnticBank(0),
    CPUAccess(false), AnticAccess(false),
    PIABankBits(2)
{
}
///

/// XEExtension::~XEExtension
// Cleanup and dispose the RAM.
XEExtension::~XEExtension(void)
{
  delete[] RAM;
}
///

/// XEExtension::MapExtension
// The following method is used to map the RAM disk into the
// RAM area. It is called by the MMU as part of the medium
// RAM area setup. We hence expect that ram extensions are part
// of the 0x4000..0x8000 area, which is true for all RAM extensions
// the author is aware of. If this call returns false, no RAM disk
// is mapped into the area and the MMU has to perform the mapping
// of default RAM. Further, this might be called several times
// once for ANTIC and once for the CPU. The antic argument tells
// us whether we map for CPU or antic.
bool XEExtension::MapExtension(class AdrSpace *adr,bool forantic)
{
  if (forantic) {
    // Ok, we have to map the extra bytes for ANTIC. Hence, check
    // whether antic has access to the pages in first place.
    if (AnticAccess) {
      class RamPage *ram;
      ADR i;
      // Yes, it does. Get the bank offset, then map banks into
      // 0x4000 and up. These are 16K, making 64 pages or a left-
      // shift by six.
      ram = RAM + (AnticBank << 6);
      for(i = 0x4000;i<0x8000; i += PAGE_LENGTH) {
	adr->MapPage(i,ram);
	ram++;
      }
      return true;
    }
  } else {
    // CPU Access. Check whether the CPU should access the additional
    // pages. Otherwise, as above.
    if (CPUAccess) {
      class RamPage *ram;
      ADR i;
      ram = RAM + (CPUBank << 6);
      for(i = 0x4000;i<0x8000; i += PAGE_LENGTH) {
	adr->MapPage(i,ram);
	ram++;
      }
      return true;
    }
  }
  return false;
}
///

/// XEExtension::PIAWrite
// Configure the RAM extension by writing into PIA.
// Get the new page modes, and possibly run the MMU to
// reconfigure this RAM area.
bool XEExtension::PIAWrite(UBYTE &data)
{
  bool cpu,antic;
  int bank,i;

  cpu   = (data & 0x10)?(false):(true); // get the antic access bit
  antic = (data & 0x20)?(false):(true); // get the cpu access bit
  //
  // Get the addressed bank by now.
  bank  = 0;
  for(i=0;i<PIABankBits;i++) {
    int mask = 1<<BankBits[i]; // The mask to the bit we currently test.
    if (data & mask) {
      // Bit is set, address the page here.
      bank  |= (1<<i);
    }
    // Now check whether this bit influences somehow with the
    // access bits. If so, we need to work around.
    if (mask & 0x10) {
      // Cludges with CPU access. If so, grant the CPU the
      // access always.
      cpu = true;
    }
    if (mask & 0x20) {
      // Cludges with ANTIC access. If so, sacrifice separate
      // antic access and let antic address the same page as
      // the cpu.
      antic = cpu;
    }
    //
    if (mask & 0x80) {
      // Steal the bit by forwarding the default to PIA, namely
      // a set bit. This cludge only works if CPU banking is enabled
      if (cpu)
	data |= 0x80;
    }
  }

  if (bank != CPUBank   || bank  != AnticBank ||
      cpu  != CPUAccess || antic != AnticAccess) {
    // Remapping required. First copy the registers back.
    CPUBank     = bank;
    AnticBank   = bank;
    CPUAccess   = cpu;
    AnticAccess = antic;
    MMU->BuildMedRam();
  }

  // Yes, this does something!
  return true;
}
///

/// XEExtension::ColdStart
// Reset the RAM extension. This should reset the banking.
void XEExtension::ColdStart(void)
{
  int i,pages;

  pages = (1<<PIABankBits)<<6;
  // Clear memory pages to really emulate a coldstart
  for(i=0;i<pages;i++) {
    RAM[i].Blank();
  }
  WarmStart();
}
///

/// XEExtension::WarmStart
// Reset the RAM extension. This should reset the banking.
void XEExtension::WarmStart(void)
{
  CPUAccess   = false;
  AnticAccess = false;
  CPUBank     = 0;
  AnticBank   = 0;
}
///

/// XEExtension::ParseArgs
// Parse the configuration of the RAM disk. This is called as
// part of the MMU setup and should hence not define a new
// title. 
void XEExtension::ParseArgs(class ArgParser *args)
{
  LONG bits = PIABankBits;
  
  args->DefineLong("XEBankBits","number of utilized PIA Port B bits for bank switching",0,8,bits);
  //
  // Check whether the number of bits changed. If so, then we need to
  // reallocate the bank memory.
  if (bits != PIABankBits) {
    int i,banks = 1<<bits;
    PIABankBits = bits;
    //
    // Rebuild the RAM now. This also looses its contents.
    delete[] RAM;
    RAM = NULL;
    RAM = new RamPage[banks<<6];
    //  
    // Reset all pages.
    for(i=0;i < banks;i++) {
      RAM[i].Blank();
    } 
    // This requires a cold-start since we invalidate memory.
    args->SignalBigChange(ArgParser::ColdStart);
  }
}
///

/// XEExtension::State
// Load/save the machine state of the RAM within here.
// This is part of the saveable interface.
void XEExtension::State(class SnapShot *snap)
{
  int i,pages;
  char id[32],helptxt[80];

  snap->DefineTitle("XEBanking");
  snap->DefineBool("GrantCPUAccess"  ,"grant the CPU access to the extended pages",CPUAccess);
  snap->DefineBool("GrantANTICAccess","grant ANTIC access to the extended pages",AnticAccess);
  snap->DefineLong("ActiveBank"      ,"currently active bank",0,255,CPUBank);
  // In case we just loaded the configuration, let the MMU rebuild the relevant parts
  // of the memory map.
  AnticBank = CPUBank;
  MMU->BuildMedRam();
  //
  // Now save the RAM contents.
  snap->DefineTitle("XERAM");
  pages = (1<<PIABankBits)<<6;
  for(i = 0;i<pages;i++) {
    snprintf(id,31,"Page%d",i);
    snprintf(helptxt,79,"130 XE extra RAM page %d contents",i);
    snap->DefineChunk(id,helptxt,RAM[i].Memory(),256);
  }
}
///

/// XEExtension::DisplayStatus
// Display the machine state of this extension for the monitor.
// This is called as part of the MMU status.
void XEExtension::DisplayStatus(class Monitor *monitor)
{
  int bankmask,i;

  // Compute the mask of all bits within PIA port B
  // that are responsible for bankswitching.
  bankmask = 0x00;
  for(i=0;i<PIABankBits;i++) {
    bankmask |= 1<<BankBits[i];
  }

  monitor->PrintStatus("\tXE banks CPU access     : %s\n"
		       "\tXE banks ANTIC access   : %s\n"
		       "\tXE number of banks bits : " LD "\n"
		       "\tXE PIA Port B bank mask : 0x%02x\n"
		       "\tXE active bank          : %d\n",
		       (CPUAccess)?("on"):("off"),
		       (AnticAccess)?("on"):("off"),
		       PIABankBits,
		       bankmask,
		       CPUBank);
}
///

