/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: pdevice.cpp,v 1.16 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: P: emulated device
 **********************************************************************************/

/// Includes
#include "pdevice.hpp"
#include "machine.hpp"
#include "new.hpp"
///

/// Statics
#ifndef HAS_MEMBER_INIT
const int PDevice::MaxBufLen = 256;
#endif
///

/// PDevice::PDevice
PDevice::PDevice(class Machine *mach,class PatchProvider *p)
  : Device(mach,p,'P','P')
{
  int i;

  for(i=0;i<8;i++) {
    PChannel[i]         = NULL;
  }
}
///

/// PDevice::~PDevice
PDevice::~PDevice(void)
{
  int i;
  for(i=0;i<8;i++) {
    delete PChannel[i];
  }
}
///

/// PDevice::Open
// Open a stream to the printer, return an error code
UBYTE PDevice::Open(UBYTE channel,UBYTE unit,char *,UBYTE aux1,UBYTE)
{
  struct PBuffer *pb = PChannel[channel];
  
  // Check whether this is unit 1. We only support one printer.
  if (unit != 1) {
    return 0x82;
  }
  
  // If the unit is already open, fail.
  if (pb) {
    return 0x81; // channel is open already
  }
  //
  //
  if ((aux1 & 0x08) == 0) {
    // Must be some kind of output open.
    return 0x87;
  }
  //
  // Get the linkage to the printer
  printer           = Machine->Printer();
  //
  pb                = new struct PBuffer;
  PChannel[channel] = pb;
  //
  return 0x01;
}
///

/// PDevice::Close
UBYTE PDevice::Close(UBYTE channel)
{
  struct PBuffer *pb = PChannel[channel];
  UBYTE res = 0x01;

  if (pb) {
    // Now flush the remaining output
    if (pb->bufsize) {
      if (!printer->PrintCharacters(pb->buffer,pb->bufsize))
	res = 0x8a;
    }
    delete pb;
    PChannel[channel] = NULL;
  }
  
  return res;
}
///

/// PDevice::Get
// Read a character from the printer. This is obviously not possible
UBYTE PDevice::Get(UBYTE,UBYTE &)
{
  return 0x92;
}
///

/// PDevice::Put
// Print a character
UBYTE PDevice::Put(UBYTE channel,UBYTE value)
{
  struct PBuffer *pb = PChannel[channel];
  UBYTE res = 0x01;

  if (pb) {
    // Check whether there is still some space in the printer
    // buffer. If not, flush it now.
    if (pb->bufsize >= MaxBufLen) {
      if (!printer->PrintCharacters(pb->buffer,pb->bufsize))
	res = 0x8a;
      pb->bufsize = 0;
    }
    pb->buffer[pb->bufsize++] = value;
  } else {
    res = 0x85;
  }
  
  return res;
}
///

/// PDevice::Status
UBYTE PDevice::Status(UBYTE)
{
  // There is nothing we may stat here.
  // Just return an "is fine" state
  return 0x01;
}
///

/// PDevice::Special
// Special purpose XIO commands go here.
UBYTE PDevice::Special(UBYTE,UBYTE,class AdrSpace *,UBYTE,
		       ADR,UWORD,UBYTE [6])
{
  // The printer knows none of them.
  return 0x92;
}
///

/// PDevice::Reset
// Reset the printer by forgetting all contents
void PDevice::Reset(void)
{
  int ch;

  for(ch=0;ch<8;ch++) {
    delete PChannel[ch];
    PChannel[ch] = NULL;
  }
}
///
