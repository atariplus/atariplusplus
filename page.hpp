/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: page.hpp,v 1.14 2003/02/01 20:32:02 thor Exp $
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

/// Defines
#define PAGE_MASK 0xff // mask out the page from an address
#define PAGE_SHIFT 0x08 // shift to get the page index
#define PAGE_LENGTH 0x100 // length of a page in bytes
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
  virtual bool ComplexWrite(ADR mem,UBYTE value) = 0;
  //
public:
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
    if (memory) {
      return memory[mem & PAGE_MASK];
    } else {
      return ComplexRead(mem);
    }
  }
  //
  // Write a byte to a page, return the "vsync" flag to indicate whether the
  // CPU should wait to the end of the scan line
  bool WriteByte(ADR mem,UBYTE val)
  {
    if (memory) {
      memory[mem & PAGE_MASK] = val;
      return false;
    } else {
      return ComplexWrite(mem,val);
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
};
///

///
#endif
