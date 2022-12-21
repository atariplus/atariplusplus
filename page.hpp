/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: page.hpp,v 1.19 2022/12/20 16:35:48 thor Exp $
 **
 ** In this module: Definition of an abstract page, keeping 256 in common
 **********************************************************************************/

#ifndef PAGE_HPP
#define PAGE_HPP

/// Includes
#include "types.hpp"
#include "exceptions.hpp"
#include "stdio.hpp"
///

/// Class Page
// Defines a single page of memory or memory mapped IO
class Page {
protected:
  // If the following pointer is valid, then this is a standard memory page.
  // This allows us to inline accesses to it which makes this a bit faster.
  UBYTE *memory;
  //
  // The following two are defined for memory mapped IO or for ROM access
  // or whatever.
  virtual UBYTE ComplexRead(ADR mem) = 0;
  virtual void  ComplexWrite(ADR mem,UBYTE value) = 0;
  //
public:
  //
  // A couple of constants to ease access to pages.
  enum {
    Page_Mask   = 0xff, // mask out the page from an address
    Page_Shift  = 8,    // shift to get the page index
    Page_Length = 0x100
  };
  //
  // Constructors and destructors. 
  Page(void)
    : memory(NULL)
  { }
  // Ditto, a constructor with memory. We do not delete it ourselfes, though.
  Page(UBYTE *mem)
    : memory(mem)
  { }
  //
  virtual ~Page(void)
  { }
  //
  // Memory access functions. These either try to read the build-in
  // memory type, or post the access to the "complex" function to
  // emulate chip accesses. The write function returns the "VSYNC"
  // flag that gets set as soon as the CPU needs to syncronize to
  // the horizontal scan.
  //
  // Read a byte. Returns the byte read.
  UBYTE ReadByte(ADR mem)
  {
#if 0
    if (memory) {
      return memory[mem & Page_Mask];
    } else {
      return ComplexRead(mem);
    }
#else
    return ComplexRead(mem);
#endif
  }
  //
  // Write a byte to a page, return the "vsync" flag to indicate whether the
  // CPU should wait to the end of the scan line
  void WriteByte(ADR mem,UBYTE val)
  {
    if (memory) {
      memory[mem & Page_Mask] = val;
    } else {
      ComplexWrite(mem,val);
    }
  }
  //
  // Return a pointer to the page memory if possible.
  UBYTE *Memory(void) const
  {
    return memory;
  }
  //
  // Read a page from an external file
  bool ReadFromFile(FILE *file);
  // Write a page to an external file
  bool WriteToFile(FILE *file);
  //
  // Patch a byte into a ROM. Generate an error for all other pages
  virtual void PatchByte(ADR,UBYTE)
  {
    Throw(NotImplemented,"Page::PatchByte","internal error");
  }
  //
  // Return an indicator whether this is an I/O area or not.
  // This is used by the monitor to check whether reads are harmless
  virtual bool isIOSpace(ADR) const
  {
    if (memory) {
      return false;
    }
    return true;
  }
};
///

///
#endif
