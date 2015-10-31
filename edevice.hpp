/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: edevice.hpp,v 1.2 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: E: emulated device for stdio editor access (no curses)
 **********************************************************************************/

#ifndef EDEVICE_HPP
#define EDEVICE_HPP

/// Includes
#include "device.hpp"
#include "stdio.hpp"
///

/// Forwards
class PatchProvider;
class CPU;
class AdrSpace;
///

/// Class EDevice
// This device implements the CIO emulation layer for the E: device
// driver, an emulation layer for stdio driven editor input and output
class EDevice : public Device {
  //
  // The device under which this has been patched in
  UBYTE   device;
  //
  // Print the character c safely to screen, return an
  // error code.
  UBYTE SavePut(char c);
  //
  // Implementations of the device abstraction layer
  // The result code is placed in the Y register and the negative flag
  // is set accordingly.
  virtual UBYTE Open(UBYTE channel,UBYTE unit,char *name,UBYTE aux1,UBYTE aux2);
  virtual UBYTE Close(UBYTE channel);
  // Read a value, return an error or fill in the value read
  virtual UBYTE Get(UBYTE channel,UBYTE &value);
  virtual UBYTE Put(UBYTE channel,UBYTE value);
  virtual UBYTE Status(UBYTE channel);
  virtual UBYTE Special(UBYTE channel,UBYTE unit,class AdrSpace *adr,UBYTE cmd,
			ADR mem,UWORD len,UBYTE aux[6]);
  //
  virtual void Reset(void);
  //
  // Translate an Unix error code to something Atari like
  static UBYTE AtariError(int error);
  //
  // Jump to the given location after completing the code
  void JumpTo(ADR dest) const;
  //
public:
  //
  EDevice(class Machine *mach,class PatchProvider *p,UBYTE device);
  virtual ~EDevice(void);
};
///

///
#endif
