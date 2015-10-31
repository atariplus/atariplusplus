/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: pdevice.hpp,v 1.14 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: P: emulated device
 **********************************************************************************/

#ifndef PDEVICE_H
#define PDEVICE_H

/// Includes
#include "device.hpp"
#include "printer.hpp"
#include "new.hpp"
///

/// Forwards
class PatchProvider;
///

/// Class PDevice
// This device implements the CIO emulation layer for the P: device
// driver
class PDevice : public Device {
public:
  static const int MaxBufLen INIT(256);
  //
  //
private:
  // Link to the printer device driver is here.
  class Printer *printer; 
  //
  // We second-buffer printer output here
  struct PBuffer {
    UBYTE *buffer;
    int    bufsize;
    //
    PBuffer(void)
      : buffer(new UBYTE[MaxBufLen]), bufsize(0)
    { 
    }
    //
    ~PBuffer(void)
    { 
      delete[] buffer;
    }
  }       *PChannel[8];
  //
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
public:
  PDevice(class Machine *mach,class PatchProvider *p);
  virtual ~PDevice(void);
};
///

///
#endif
