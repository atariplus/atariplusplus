/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atarisioport.hpp,v 1.14 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: This is the controlling unit for Matthias Reichl's
 ** atarisio interface. This class keeps the file handle for AtariSIO.
 **
 ** User space SIO interface of AtariSIO included here under TPL with the kind
 ** permission of Matthias Reichl.
 **********************************************************************************/

#ifndef ATARISIOPORT_HPP
#define ATARISIOPORT_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "configurable.hpp"
#include "chip.hpp"
#include "exceptions.hpp"
#include "timer.hpp"
#include "serialdevice.hpp"
#include "stdio.hpp"
#include "serialstream.hpp"
#ifdef HAVE_ATARISIO_H
#include <stdint.h>
#include <atarisio.h>
#endif
///

/// Forward references
class Monitor;
class Chip;
///

/// Class AtariSIOPort
// This class keeps the file handle for the AtariSIO
// interface and also translates error codes.
class AtariSIOPort : public Chip {
  //
  // The file identifier we use for AtariSIO here.
  int                 SioFD;
  // The interface to the Os independent serial port.
  class SerialStream *SerialStream;
  //
  // Enable or disable this interface
  bool         EnableSIO;
  //
  // Enable or disable direct SIO access
  bool         DirectSerial;
  //
  // Configuration issue: Whether this is a ProSystem cable (then true)
  // or a 1050-to-PC cable (then false).
  bool         ProSystem;
  //
  // Name of the device we output the data to. This is the name of
  // the game.
  char        *DeviceName;
  //
  // usecs from start of the Command line assertion to the start
  // of the data transfer
  LONG         CmdToDataDelay;
  //
  // usecs from the start of the data transfer to the end of the
  // lowering of Cmd, thus the duration of a command frame.
  LONG         CmdFrameDelay;
  //
  // A Timer for precise timing of the serial transfer.
  class Timer  SerialTime;
  // The following boolean is set if the serial command frame
  // has not yet been completed timing-vise.
  bool         WaitCmdFrame;
  //
  // Set the status of the COMMAND line, either on or off.
  void SetCommandLine(bool onoff);
  // Flush the contents of the serial output buffer; this
  // disposes the buffer contents.
  void Flush(void);
  // Drain the output buffer. This waits until all the buffer
  // contents has been written out.
  void Drain(void);
  // Delay the given number of microseconds for proper
  // timing of the serial port.
  void Delay(ULONG usecs);
  //
  // Re-open the SIO buffer and initialize it
  void OpenChannel(void);
  //
  // Convert an AtariOs error to a more informative string
  static const char *ErrorString(int error);
  //
public:
  AtariSIOPort(class Machine *mach);
  ~AtariSIOPort(void);
  //
  // Warm and coldstart this chip.
  virtual void WarmStart(void);
  virtual void ColdStart(void);
  //
  // Transmit a command to an external device by means
  // of the kernel interface. Returns the result character
  // of the external device.
  // The first boolean describes the data direction. Unfortunately,
  // AtariSIO knows only IN or OUT, not STATUS.
  // The second array is the command frame,
  // the next the source or the target buffer, the last the byte
  // size. Timeout is timeout in seconds.
  char External(bool writetodevice,const UBYTE *commandframe,UBYTE *buffer,int size,UBYTE timeout);
  //
  // Check whether we are in direct serial mode. If so, then buffers are transmitted byte by
  // byte
  bool DirectMode(void) const
  {
    return (DirectSerial && EnableSIO);
  }
  //
  // Direct mode IO: Read/Write single bytes.
  // First write a single byte. Creates a warning in case we can't.
  void WriteDirectByte(UBYTE byte)
  {
    if (DirectSerial && EnableSIO && SerialStream) {
      if (WaitCmdFrame) {
	// Command frame is not yet completed, wait for
	// its completion.
	// Ok, be a bit nasty. The kernel doesn't allow us to wait for
	// less than 10ms savely, thus busy wait. Urgl!
	while(SerialTime.EventIsOver() == false){};
	WaitCmdFrame = false;	
	// Deactivate the command line now.
	SetCommandLine(false);
      }
      if (SerialStream->Write(&byte,1) <= 0) {
	ThrowIo("AtariSIOPort::WriteDirectByte","failed to output a byte thru the serial");
      }
    }
  }
  // Read a single byte. Return a counter how many bytes are 
  // read (at most one, or zero).
  UBYTE ReadDirectByte(UBYTE &byte)
  {
    if (DirectSerial && EnableSIO && SerialStream) {
      LONG len;
      if (WaitCmdFrame) { 
	//fprintf(stderr,"w");
	// Command frame is not yet completed, wait for
	// its completion.
	// Ok, be a bit nasty. The kernel doesn't allow us to wait for
	// less than 10ms savely, thus busy wait. Urgl!
	while(SerialTime.EventIsOver() == false){};
	WaitCmdFrame = false;
	// Deactivate the command line now.
	SetCommandLine(false);
      }
      len = SerialStream->Read(&byte,1);
      if (len < 0) {
	ThrowIo("AtariSIOPort::ReadDirectByte","failed to read a byte from the serial port");
      }
      /*
      if (len > 0)
	fprintf(stderr,"%02x",byte);
      */
      return UBYTE(len);
    }
    return 0;
  }
  // Transmit a command frame in direct IO.
  void TransmitCommandFrame(const UBYTE *cmdframe);
  //
  // Parse off command line arguments.
  virtual void ParseArgs(class ArgParser *args);
  //
  // Display the status of this device.
  virtual void DisplayStatus(class Monitor *mon);
};
///

///
#endif

