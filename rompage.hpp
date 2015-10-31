/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: rompage.hpp,v 1.11 2015/10/25 09:11:23 thor Exp $
 **
 ** In this module: Definition of a page of ROM
 **********************************************************************************/

#ifndef ROMPAGE_HPP
#define ROMPAGE_HPP

/// Includes
#include "types.hpp"
#include "page.hpp"
#include "new.hpp"
///

/// Class RomPage
// Defines a single page of read only memory
class RomPage : public Page {
protected:  
  //
  // The ROM image goes here. We *must not* use the
  // "memory" pointer of the page as this would allow
  // write accesses to the rom.
  UBYTE *romimage;
  //
  virtual UBYTE ComplexRead(ADR mem)
  {
    return romimage[mem & PAGE_MASK];
  }
  //
  virtual void ComplexWrite(ADR,UBYTE)
  {
    // Writes to ROM are not allowed and are silently ignored
  }
  //
public:
  //
  // The constructor also constructs the memory here.
  RomPage(void)
    : romimage(new UBYTE[256])
  { }
  //
  ~RomPage(void)
  {
    delete[] romimage;
  }
  //
  //
  // We overload the memory access functions such that we have faster
  // access if the compiler is smart enough. Maybe it isn't.
  // Read a byte. Returns the byte read.
  UBYTE ReadByte(ADR mem)
  {
    return romimage[mem & PAGE_MASK];
  }
  //
  // Write a byte to a page.
  void WriteByte(ADR,UBYTE)
  {    
  }
  //
  // Blank a rompage to all zeros
  void Blank(void)
  {
    memset(romimage,0,256);
  }  
  //
  // Patch a byte into a ROM. 
  virtual void PatchByte(ADR mem,UBYTE val)
  {
    romimage[mem & PAGE_MASK] = val;
  }
  //
  // Return an indicator whether this is an I/O area or not.
  // This is used by the monitor to check whether reads are harmless
  virtual bool isIOSpace(ADR) const
  {
    return false;
  }
};
///

///
#endif
