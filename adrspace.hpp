/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: adrspace.hpp,v 1.22 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Definition of the complete 64K address space of the emulator
 **********************************************************************************/

#ifndef ADRSPACE_HPP
#define ADRSPACE_HPP

/// Includes
#include "string.hpp"
#include "stdlib.hpp"
#include "types.hpp"
#include "exceptions.hpp"
#include "page.hpp"

#if DEBUG_LEVEL > 0
#include "main.hpp"
#endif
///

/// Class AdrSpace
// Definition of the complete address space of the Atari,
// that is, 64K of memory.
// The AdrSpace does not control the life-time of the memory,
// this is the matter of the MMU class
class AdrSpace {
  //
  // The address space consists of 64K of memory, that is, 256
  // pages of 256 bytes each.
  class Page *pages[256];
  //
public:
  AdrSpace(void)
  {
    memset(pages,0,sizeof(class Page *) * 256);
  }
  //
  // Map a page to a given address
  void MapPage(ADR mem,class Page *page)
  {
#if CHECK_LEVEL > 2
    if ((mem & Page::Page_Mask) != 0)
      Throw(InvalidParameter,"Page::MapPage","Page address is not aligned");
#endif
    pages[mem >> Page::Page_Shift] = page;
  }
  //
  // Read and write from an address
  UBYTE ReadByte(ADR mem)
  {
#if CHECK_LEVEL > 1
    if (mem < 0 || mem > 0xffff) {
      Throw(OutOfRange,"Page::ReadByte","Address is invalid");
    }
    if (pages[mem >> Page::Page_Shift] == NULL)
      Throw(ObjectDoesntExist,"Page::ReadByte","Page is undefined");
#endif
    return pages[mem >> Page::Page_Shift]->ReadByte(mem);
  }
  //
  // Write to an address.
  void WriteByte(ADR mem,UBYTE val)
  {
#if CHECK_LEVEL > 1
    if (mem < 0 || mem > 0xffff) {
      Throw(OutOfRange,"Page::WriteByte","Address is invalid");
    }
    if (pages[mem >> Page::Page_Shift] == NULL)
      Throw(ObjectDoesntExist,"Page::WriteByte","Page is undefined");
#endif
    pages[mem >> Page::Page_Shift]->WriteByte(mem,val);
  } 
  //
  // Patch a ROM entry. This works only for ROM patches
  // and does nothing for RAM.
  void PatchByte(ADR mem,UBYTE val)
  {
#if CHECK_LEVEL > 1
    if (mem < 0 || mem > 0xffff) 
      Throw(OutOfRange,"Page::PatchByte","Address is invalid");
    if (pages[mem >> Page::Page_Shift] == NULL)
      Throw(ObjectDoesntExist,"Page::PatchByte","Page is undefined");
#endif
    pages[mem >> Page::Page_Shift]->PatchByte(mem,val);
  }
  //
  //
  // Similar methods to read and write entire WORDs in the
  // 6502 order (little endian)
  UWORD ReadWord(ADR mem)
  {
    UWORD data;
#if CHECK_LEVEL > 1
    if (mem < 0 || mem > 0xfffe) 
      Throw(OutOfRange,"Page::ReadWord","Address is invalid");
    if (pages[mem >> Page::Page_Shift] == NULL || pages[(mem+1) >> Page::Page_Shift] == NULL)
      Throw(ObjectDoesntExist,"Page::ReadWord","Page is undefined");
#endif
    data  = pages[mem     >> Page::Page_Shift]->ReadByte(mem);
    data |= UWORD(pages[(mem+1) >> Page::Page_Shift]->ReadByte(mem+1)) << 8;
    //
    return data;
  }
  //
  // Exclusively for the CPU: Return the Z-page memory which is never mapped
  // or IO space
  UBYTE *ZeroPage(void) const
  {
    return pages[0]->Memory();
  }
  //
  // Exclusively for the CPU: Return the stack 
  UBYTE *StackPage(void) const
  {
    return pages[1]->Memory();
  }
  //
  // Check whether a byte belongs to a hardware register. If so,
  // it cannot be read without a side effect.
  bool isIOSpace(ADR mem) const
  {
#if CHECK_LEVEL > 1
    if (mem < 0 || mem > 0xffff) {
      Throw(OutOfRange,"Page::isIOSpace","Address is invalid");
    }
    if (pages[mem >> Page::Page_Shift] == NULL)
      Throw(ObjectDoesntExist,"Page::isIOSpace","Page is undefined");
#endif
    return pages[mem >> Page::Page_Shift]->isIOSpace(mem);
  }
};
///

///
#endif
