/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: rdevice.hpp,v 1.4 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: R: emulated device
 **********************************************************************************/

#ifndef RDEVICE_H
#define RDEVICE_H

/// Includes
#include "device.hpp"
#include "new.hpp"
#include "hbiaction.hpp"
///

/// Forwards
class PatchProvider;
class InterfaceBox;
///

/// Class RDevice
// This device implements an alternative (native) CIO emulation
// layer for the RS232-interface, the R: handler.
class RDevice : public Device, private HBIAction {
  //
  // Link to the interface box that performs the real IO is here
  class InterfaceBox *serial;
  //
  // IO buffer for emulated SIO communication.
  UBYTE              *buffer;
  // Allocated size of the buffer.
  int                 buffersize;
  //
  // R: internal buffer for reading.
  UBYTE              *inputbuffer;
  //
  // Flag: Is the interface channel open or not?
  bool                isopen;
  // Flag: Concurrent mode active?
  bool                concurrent;
  // Flag: Parity error detected?
  bool                parityerror;
  // Flag: Buffer overrung detected?
  bool                overrun;
  //
  // The mode for which this channel has been opened.
  UBYTE               openmode;
  // The number of data bits of the communication.
  UBYTE               databits;
  // Character transposition (ASCII,ATASCII conversion?)
  UBYTE               transposition;
  // Replacement character for invalid ASCII input
  UBYTE               invreplace;
  //
  // In case the user supplied an input buffer, we "magically" place read
  // data into this buffer per "DMA" bypassing the CPU.
  ADR                 dmabuffer;
  UWORD               dmabuflen; // if non-zero, this buffer is available.
  class AdrSpace     *cpumem;
  //
  // Number of bytes in the input buffer
  UWORD               bufferedbytes;
  // Insertion position into the buffer.
  UWORD               insertpos;
  // Removing end of the buffer.
  UWORD               removepos;
  //
  //
  // Methods follow:
  //
  // Enlarge the input/output buffer to at least the indicated amount of bytes
  void EnlargeBuffer(int minsize);
  //
  // Write a single byte out to the serial output buffer,
  // return the status.
  UBYTE PutByte(UBYTE value);
  //
  // Forward a SIO command to the interface box, returns the CIO
  // error code.
  UBYTE RunCommand(UBYTE cmd,UBYTE aux1,UBYTE aux2,int &size);
  //
  // Compute the parity for the input data with the indicated number of bits.
  // Returns whether the number of one-bits is even (then false) or odd (then true).
  bool ComputeParity(UBYTE value);
  //
  // The following HBI is used to pull bytes from the serial interface and
  // place them into the buffer.
  virtual void HBI(void);
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
  RDevice(class Machine *mach,class PatchProvider *p);
  virtual ~RDevice(void);
};
///

///
#endif
