/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: amdchip.cpp,v 1.5 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: Emulation of AMD FlashROM chips by Mark Keates
 **********************************************************************************/

/// Includes
#include "cartflash.hpp"
#include "flashpage.hpp"
#include "mmu.hpp"
#include "amdchip.hpp"
#include "monitor.hpp"
#include "snapshot.hpp"
///

/// AmdChip::AmdChip
// Constructor
AmdChip::AmdChip(class Machine *mach,ChipType ct,const char *name,int unit,class CartFlash *cf)
  : Chip(mach,name), Saveable(mach,name,unit), cmdState(CmdRead), type(ct), 
    Modified(false), Enabled(true), Unit(unit), Parent(cf), Rom(NULL)
{
  int i;
  //
  // Compute the number of banks the chip provides.
  switch(ct) {
  case Am010:
    TotalBanks = 0x10;
    break;
  case Am020:
    TotalBanks = 0x20;
    break;
  case Am040:
    TotalBanks = 0x40;
    break;
  default:
    TotalBanks = 0x00;
    break;
  }
  TotalPages   = TotalBanks << 5;
  ActiveBank   = (type == Am010) ? 0 : TotalBanks-1;
  //
  // Allocate the pages for the chip
  if (TotalPages > 0) {
    Rom = new class FlashPage*[TotalPages];
    //
    // Erase all the pages we hold.
    for(i = 0;i < TotalPages;i++) {
      Rom[i] = NULL;
    }
  }
}
///

/// AmdChip::Initialize
// MemController interfaces: Setup the contents of this chip.
void AmdChip::Initialize(void)
{
  // This finally allocates all memory pages.
  if (Rom) {
    int i;
    //
    for(i = 0;i < TotalPages;i++) {
      if (Rom[i] == NULL)
	Rom[i] = new FlashPage(this);
    }
  }
}
///

/// AmdChip::~AmdChip
AmdChip::~AmdChip(void)
{
  if (Rom) {  
    int i;
    //
    // Erase all ROM pages we have.
    for(i = 0;i < TotalPages;i++) {
      delete Rom[i];
    }
    delete Rom;
  }
}
///

/// AmdChip::ColdStart
// Coldstart of this chip, reset all we got.
void AmdChip::ColdStart(void)
{
  WarmStart();
}
///

/// AmdChip::WarmStart
void AmdChip::WarmStart(void)
{
  // Set the initial state of the state machine and the page to
  // be read from intially. Depends on the chip type.
  ActiveBank = (type == Am010) ? 0 : TotalBanks-1;
  cmdState   = CmdRead;
}
///

/// AmdChip::ChipErase
// Cleanup the contents of all banks, or a selected sector.
void AmdChip::ChipErase(UWORD sector)
{
  int i;
  UWORD page_start=0, page_end=TotalPages;

  Modified = true;   
  if (sector != UWORD(~0)) {
    int sector_size; // pages
    switch (type) {
    case Am010:
      sector_size = 0x40;
      page_start  = sector << 6; // Sectors are 64 pages = 16K large
      page_end    = page_start + sector_size - 1;
      break;
    case Am040:
      sector_size = 0x100;
      page_start  = sector << 8; // Sectors are 256 pages = 64K large
      page_end    = page_start + sector_size - 1;
      break;
    default:
      sector_size = 0x00;
      page_start  = page_end = 0;
    }
  }

  for (i=page_start; i<page_end; i++) 
    Rom[i]->Blank();
}
///

/// AmdChip::RomAreaWrite
// Write data into the cart area
bool AmdChip::RomAreaWrite(ADR mem,UBYTE val)
{
  class FlashPage *fp;
  UBYTE b;
  ADR m;
  // Writes to ROM are not allowed and are silently ignored
  //printf("CartFlash::RomAreaWrite : %04X=$%02X (%d)\n", mem, val, cmdState);
  m = (ActiveBank & 7) << 13;
  m = m | (mem & 0x1FFF);
  if (cmdState == CmdAutoSelect) 
    cmdState = CmdRead;
  //
  // Ok, advance the state machine of the chip.
  switch (cmdState)
    {
    case CmdRead:
      if (m == 0x5555 && val == 0xAA)
	cmdState = CmdSecond;
      break;
    case CmdSecond:
      if (m == 0x2AAA && val == 0x55)
	cmdState = CmdThird;
      break;
    case CmdThird:
      // Finally, select the chip function.
      if (m == 0x5555) {
	switch (val) {
	case 0x90:
	  cmdState = CmdAutoSelect;
	  break;
	case 0xA0:
	  cmdState = CmdProgram;
	  break;
	case 0x80:
	  cmdState = CmdErase1;
	  break;
	}
      }
      break;
    case CmdProgram:
      fp = Rom[(ActiveBank<<5)+((mem-0xA000)>>PAGE_SHIFT)];
      b  = fp->ReadByte(mem);
      fp->PatchByte(mem, b & val); // The flash process can only reset bytes.
      //printf("Post-Programmed Mem : %04X=%02X (%02X)\n", mem, fp->ReadByte(mem), b);
      cmdState = CmdRead;
      Modified = true;
      break;
    case CmdErase1:
      if (m == 0x5555 && val == 0xAA)
	cmdState = CmdErase2;
      break;
    case CmdErase2:
      if (m == 0x2AAA && val == 0x55)
	cmdState = CmdErase;
      break;
    case CmdErase:
      // Finally, trigger the erase function of the chip
      if (m == 0x5555 && val == 0x10) {
	// Erase all of the chip
	ChipErase();
	Modified = true;
	cmdState = CmdRead;
      } else if (/*m == 0x5555 && */ val == 0x30) { 
	// Interestingly, this doesn't have to check for the address.
	//
	// Erase only a specific sector
	switch(type) {
	case Am010:
	  ChipErase(((ActiveBank & 1)<<2) | ((mem>>13) & 3)); // 16K
	  break;
	case Am040:
	  ChipErase((ActiveBank>>4) & 7); // 64K
	  break;
	default:
	  // Huh, no sector erase for the Am020?
	  break;
	}
	cmdState = CmdRead;
      }
      break;
    default:
      if (val == 0xF0) 
	cmdState = CmdRead;
    }
  return false;
}
///

/// AmdChip::RomAreaRead
// Read data from the ROM area of the parent
UBYTE AmdChip::RomAreaRead(ADR mem,UBYTE val)
{
  UBYTE new_val = val;
  if (cmdState == CmdAutoSelect) {
    switch (mem & 0xFF) {
    case 0:
      // Requested manufacturer ID.
      //printf("Requested Manufacturer ID\n");
      new_val = (type != AmNone) ? 0x01 : 0x00; // AMD
      break;
    case 1:
      // Requested device ID
      //printf("Requested Device ID\n");
      switch (type) {
      case Am010:
	new_val = 0x20;
	break;
      case Am040:
	new_val = 0xA4;
	break;
      default:
	new_val = 0x00;
      }
    }
  }
  //	printf("CartFlash::RomAreaRead  : %04X=$%02X (%d)\n", mem, new_val, ActiveBank);
  return new_val;
}
///

/// AmdChip::ReadFromFile
// Read the contents of this chip from a file,
// restoring a previously saved state
void AmdChip::ReadFromFile(FILE *fp)
{
  int i;
  
  for (i=0; i<TotalPages; i++) {
    if (!Rom[i]->ReadFromFile(fp))
      ThrowIo("AmdChip::ReadFromFile","failed to read the AMD FlashROM image from file");
  }
}
///

/// AmdChip::WriteToFile
// Write the data back to a file
void AmdChip::WriteToFile(FILE *fp)
{
  int i;

  for (i=0; i<TotalPages; i++) {
    if (!Rom[i]->WriteToFile(fp))
      ThrowIo("AmdChip::WriteToFile","failed to write the AMD FlashROM image to file");
  }
  // No saving required anymore.
  Modified = false;
}
///

/// AmdChip::MapChip
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool AmdChip::MapChip(class MMU *mmu,UBYTE activebank)
{
  if (Enabled && type != AmNone) {
    ADR i;
    int displacement;
    // Get the active bank and map it into 0xa000 to 0xbfff
    // 8K = 2^13
    ActiveBank   = activebank;
    displacement = (activebank << 13) - 0xa000;
    for(i=0xa000;i<0xc000;i+=PAGE_LENGTH) {
      mmu->MapPage(i,Rom[(i+displacement)>>PAGE_SHIFT]);
    }
    return true;
  }
  return false;
}
///

/// AmdChip::ParseArgs
// Parse off the arguments for this chip.
void AmdChip::ParseArgs(class ArgParser *)
{
}
///

/// AmdChip::DisplayStatus
// Display the status of this chip over the built-in monitor.
void AmdChip::DisplayStatus(class Monitor *mon)
{
  const char *ct = "",*cs = "";
  //
  // Convert type and state into a human-readable form
  // (of sorts)
  switch(type) {
  case AmNone:
    ct = "None";
    break;
  case Am010:
    ct = "Am010";
    break;
  case Am020:
    ct = "Am020";
    break;
  case Am040:
    ct = "Am040";
    break;
  }
  switch (cmdState) {
  case CmdRead:
    cs = "CmdRead";
    break;
  case CmdSecond:
    cs = "CmdSecond";
    break;
  case CmdThird:
    cs = "CmdThird";
    break;
  case CmdAutoSelect:
    cs = "CmdAutoSelect";
    break;
  case CmdProgram:
    cs = "CmdProgram";
    break;
  case CmdErase1:
    cs = "CmdErase1";
    break;
  case CmdErase2:
    cs = "CmdErase2";
    break;
  case CmdErase:
    cs = "CmdErase";
    break;
  case CmdLimbo:
    cs = "CmdLimbo";
    break;
  }
  //
  mon->PrintStatus("AmdChip status:\n"
		   "\tChip type      : %s\n"
		   "\tNumber of banks: %d\n"
		   "\tChip state     : %s\n"
		   "\tActive bank    : %d\n",
		   ct,TotalBanks,cs,ActiveBank);
}
///

/// AmdChip::State
// Make a snapshot of this chip to the state file.
void AmdChip::State(class SnapShot *sn)
{
  char id[32];
  char helptxt[80];
  int i;
  int pages = TotalBanks << 5;
  //
  sn->DefineTitle(Saveable::NameOf());
  //
  for(i = 0;i<pages;i++) {
    snprintf(id,31,"Page%d",i);
    snprintf(helptxt,79,"FlashRAM page %d contents",i);
    sn->DefineChunk(id,helptxt,Rom[i]->Memory(),256);
  }
}
///
