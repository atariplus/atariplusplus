/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: rampage.hpp,v 1.10 2022/12/20 16:35:48 thor Exp $
 **
 ** In this module: Definition of a page of memory
 **********************************************************************************/

#ifndef RAMPAGE_HPP
#define RAMPAGE_HPP

/// Includes
#include "types.hpp"
#include "page.hpp"
#include "new.hpp"
///

/// Class RamPage
// Defines a single page of real memory
class RamPage : public Page {
  //
  // The used flags for statistical purposes.
  UBYTE *UsedFlags;
  //
public:
  //
  // The constructor also constructs the memory here.
  RamPage(void)
    : Page(new UBYTE[Page::Page_Length]), UsedFlags(NULL)
  { }
  //
  ~RamPage(void)
  {
    // This is not our field, but we delete it since we constructed it.
    delete[] memory;
  }
  //
  // Tell the RAM where the used flags are (if any)
  void setUsedFlags(UBYTE *flags)
  {
    UsedFlags = flags;
  }
  //
  // We overload the memory access functions such that we have faster
  // access if the compiler is smart enough. Maybe it isn't.
  // Read a byte. Returns the byte read.
  UBYTE ReadByte(ADR mem)
  {
    if (UsedFlags)
      UsedFlags[mem & Page::Page_Mask] = 1;
    
    return memory[mem & Page::Page_Mask];
  }
  //
  // Write a byte to a page.
  void WriteByte(ADR mem,UBYTE val)
  {
    memory[mem & Page::Page_Mask] = val;
  }
  //
  // The following "complex" functions should never be called since the page
  // should fall back to direct reads/writes in first place. We present them
  // here for orthogonality
  // Read a byte. Returns the byte read.
  UBYTE ComplexRead(ADR mem)
  {
    if (UsedFlags)
      UsedFlags[mem & Page::Page_Mask] = 1;

    return memory[mem & Page::Page_Mask];
  }
  //
  // Write a byte to a page.
  void ComplexWrite(ADR mem,UBYTE val)
  {
    memory[mem & Page::Page_Mask] = val;
  }  
  //
  // Patch a byte into a RAM. 
  virtual void PatchByte(ADR mem,UBYTE val)
  {
    memory[mem & Page::Page_Mask] = val;
  }  
  //
  // Blank a rampage to all zeros: This initializes a page
  // for a coldstart.
  void Blank(void)
  {
    memset(memory,0,Page::Page_Length);
  }  
};
///

///
#endif
