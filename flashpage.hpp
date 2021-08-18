/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: flashpage.hpp,v 1.5 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Definition of a page of an AMD FlashROM
 **********************************************************************************/

#ifndef FLASHPAGE_HPP
#define FLASHPAGE_HPP

/// Includes
#include "types.hpp"
#include "cartflash.hpp"
#include "amdchip.hpp"
#include "page.hpp"
#include "new.hpp"
///

/// Class FlashPage
// Defines a single page of flash ROM as found in the AMD chips
class FlashPage : public Page {
protected:
  //
  // The ROM image goes here. We *must not* use the
  // "memory" pointer of the page as this would allow
  // write accesses to the rom.
  UBYTE         *romimage;
  //
  // The Chip itself. This chip also reacts on reads/writes into
  // the memory area (yuck!)
  class AmdChip *parent;
  //
  virtual UBYTE ComplexRead(ADR mem)
  {
    UBYTE b = romimage[mem & Page::Page_Mask];
    //
    if (parent->InterceptsRead()) {
      return parent->RomAreaRead(mem,b);
    }
    //
    return b;
  }
  //
  // Write data into the chip area.
  virtual void ComplexWrite(ADR mem,UBYTE val)
  {
    parent->RomAreaWrite(mem, val);
  }
  //
public:
  //
  // The constructor also constructs the memory here.
  FlashPage(class AmdChip *amd)
    : romimage(new UBYTE[Page::Page_Length]), parent(amd)
  {
    // Ensure the data is "erased" in the flash ROM sense.
    Blank();
  }
  //
  ~FlashPage(void)
  {
    delete[] romimage;
  }
  //
  // Return a pointer to the page memory for snapshotting. This
  // overrides Page::Memory.
  UBYTE *Memory(void) const
  {
    return romimage;
  }
  //
  // We overload the memory access functions such that we have faster
  // access if the compiler is smart enough. Maybe it isn't.
  // Read a byte. Returns the byte read.
  UBYTE ReadByte(ADR mem)
  {
    return romimage[mem & Page::Page_Mask];
  }
  //
  // Write a byte to a page. Nothing happens here.
  void WriteByte(ADR,UBYTE)
  {
  }
  //
  // Blank a FlashPage to all zeros. For the flash ROM, this means
  // all is 0xff. Writing data means setting bits to zero.
  void Blank(void)
  {
    memset(romimage,255,Page::Page_Length);
  }
  //
  // Patch a byte into a FlashROM.
  virtual void PatchByte(ADR mem,UBYTE val)
  {
    romimage[mem & Page::Page_Mask] = val;
  }
};
///

///
#endif
