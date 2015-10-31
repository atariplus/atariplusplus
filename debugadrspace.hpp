/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: debugadrspace.hpp,v 1.5 2015/05/21 18:52:38 thor Exp $
 **
 ** In this module: Definition of the complete 64K address space of the emulator
 ** this one includes debug information
 **********************************************************************************/

#ifndef DEBUGADRSPACE_HPP
#define DEBUGADRSPACE_HPP

/// Includes
#include "string.hpp"
#include "stdlib.hpp"
#include "types.hpp"
#include "exceptions.hpp"
#include "page.hpp"
#include "monitor.hpp"
#include "adrspace.hpp"

#if DEBUG_LEVEL > 0
#include "main.hpp"
#endif
///

/// Class DebugAdrSpace
// Definition of the complete address space of the Atari,
// that is, 64K of memory.
// The AdrSpace does not control the life-time of the memory,
// this is the matter of the MMU class. This class includes
// additional debug functionality to capture memory accesses.
class DebugAdrSpace {
  //
  // The address space we work on.
  class AdrSpace *Mem;
  //
  // The monitor we break into.
  class Machine  *Machine;
  //
  // The break points enabled here.
  ADR             BreakPoint[16];
  //
  // Test whether a read-hit is enough.
  bool            HitOnRead[16];
  //
  // Number of break points enabled.
  UBYTE           Count;
  //
  // Capture the indicated watch point.
  void CaptureWatch(UBYTE idx,ADR mem);
  //
  // Check whether an address is breaked on.
  void TestAddress(ADR mem)
  { 
    if (Count) {
      UBYTE i = Count;
      while(i) {
	--i;
	if (BreakPoint[i] == mem) {
	  CaptureWatch(i,mem);
	}
      }
    }
  }
  //
  // Check whether an address is breaked on.
  void TestReadAddress(ADR mem)
  { 
    if (Count) {
      UBYTE i = Count;
      while(i) {
	--i;
	if (BreakPoint[i] == mem && HitOnRead[i]) {
	  CaptureWatch(i,mem);
	}
      }
    }
  }
  //
  //
public:
  // 
  //
  DebugAdrSpace(class Machine *mach,class AdrSpace *parent)
    : Mem(parent), Machine(mach), Count(0)
  {
  }
  // Install a breakpoint at the given address, return the break
  // point number, or -1 in case no free break point is found.
  BYTE SetWatchPoint(ADR mem,bool hitonread)
  {
    //
    if (Count < 16) {
      BreakPoint[Count] = mem;
      HitOnRead[Count]  = hitonread;
      return Count++;
    } else {
      return -1;
    }
  }
  //
  // Remove the given break point by index.
  void RemoveWatchPointByIndex(UBYTE idx)
  {
    if (idx < Count) {
      UBYTE i = idx;
      while(i + 1 < Count) {
	BreakPoint[i] = BreakPoint[i+1];
	i++;
      }
      Count--;
    }
  }
  //
  // Remove the given break point by address.
  void RemoveWatchPoint(ADR mem)
  {
    if (Count) {
      UBYTE i = Count;
      while(i) {
	--i;
	if (BreakPoint[i] == mem) {
	  RemoveWatchPointByIndex(i);
	  break;
	}
      }
    }
  }
  //
  // Are any watch points enabled.
  bool WatchesEnabled(void) const
  {
    return Count > 0;
  }
  //
  // Read and write from an address
  UBYTE ReadByte(ADR mem)
  {
    TestReadAddress(mem);
    
    return Mem->ReadByte(mem);
  }
  //
  // Write to an address, return the VSYNC flag
  void WriteByte(ADR mem,UBYTE val)
  {  
    TestAddress(mem);

    Mem->WriteByte(mem,val);
  } 
  //
  // Similar methods to read and write entire WORDs in the
  // 6502 order (little endian)
  UWORD ReadWord(ADR mem)
  {
    TestReadAddress(mem);
    TestReadAddress(mem+1);

    return Mem->ReadWord(mem);
  }
};
///

///
#endif
