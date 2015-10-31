/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: debugadrspace.cpp,v 1.3 2015/05/21 18:52:38 thor Exp $
 **
 ** In this module: Definition of the complete 64K address space of the emulator
 ** this one includes debug information
 **********************************************************************************/

/// Includes
#include "machine.hpp"
#include "monitor.hpp"
#include "debugadrspace.hpp"
#include "cpu.hpp"
///

/// DebugAdrSpace::CaptureWatch
// Hit a watch point, now enter the monitor.
void DebugAdrSpace::CaptureWatch(UBYTE idx,ADR)
{
  Machine->CPU()->GenerateWatchPoint(idx);
}
///
