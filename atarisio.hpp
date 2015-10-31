/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atarisio.hpp,v 1.13 2015/08/15 14:52:35 thor Exp $
 **
 ** In this module: Support for real atari hardware, connected by 
 ** Matthias Reichl's atarisio interface.
 **********************************************************************************/

#ifndef ATARISIO_HPP
#define ATARISIO_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "atarisioport.hpp"
#include "stdio.hpp"
///

/// Forward references
class Monitor;
///

/// Class AtariSIO
// This is the interface class towards the atarisio kernel
// interface.
class AtariSIO : public SerialDevice {
  //
  // Drive number of this real drive. Starts counting at zero.
  int                    DriveId;
  //
  // Set when we are double density.
  bool                   DoubleDensity;
  //
  // Set to true if write protected.
  bool                   WriteProtected;
  //
  // Enable or disable this interface as required.
  bool                   EnableSIO;
  //
  // Timeout in seconds for regular commands.
  UBYTE                  TimeOut;
  // Timeout for format commands.
  UBYTE                  FormatTimeOut;
  // 
  // The following is set if we did not yet receive the handshaking
  // of the device after the command frame.
  bool                   ExpectCmdHandshake;
  // The following is set if we did not yet receive the handshaking
  // of the device before/after the data frame.
  bool                   ExpectDataHandshake;
  //
  // Type of the serial frame we are going to handle in direct
  // IO. This is not used for kernel space I/O.
  SIO::CommandType       CmdType;
  //
  // Checksum for direct (user space) I/O. We keep this in parallel
  // to the SIO checksum because SIO itself tries to "help" simple
  // devices by calculating the checksum on its own.
  UBYTE                  ChkSum;
  //
  // The device response code.
  UBYTE                  Response;
  //
  // The number of bytes expected for the data frame to transmit,
  // not including the checksum byte. This is only used for
  // direct (user space) I/O
  int                    DataFrameSize;
  //
  // Internal buffer for the received data frame. This is kept here
  // because this driver itself needs to know the resulting data
  // for some commands. This is only 256 bytes large as disk drives
  // do not support longer data frames anyhow.
  UBYTE                 *DataFrame;
  //
  // Pointer into the above buffer
  UBYTE                 *DataFramePtr;
  //
  // Transmit a command to an external device by means
  // of the kernel interface. Returns the result character
  // of the external device.
  // The first boolean describes the data direction. Unfortunately,
  // AtariSIO knows only IN or OUT, not STATUS.
  // The second array is the command frame,
  // the next the source or the target buffer, the last the byte
  // size.
  char External(bool writetodevice,const UBYTE *commandframe,UBYTE *buffer,int size);
  //
  // Emulate/transmit an 815 "ReadStatusBlock" command, and analyze the result
  // for the new density.
  UBYTE ReadStatusBlock(const UBYTE *cmdframe,UBYTE *buffer);
  //
  // Write out a status block to the device.
  // This could possibly change the sector size, so we need
  // to get informed about it.
  UBYTE WriteStatusBlock(const UBYTE *cmd,const UBYTE *buffer,int size);
  //
  // Interpret a 815 status block and change the density if
  // required.
  void AdaptDensity(const UBYTE *dataframe);
  //
  //
public:
  // Constructors and destructors
  AtariSIO(class Machine *mach,const char *name,int id);
  ~AtariSIO(void);
  //
  // Methods required by SIO  
  // Check whether this device accepts the indicated command
  // as valid command, and return the command type of it.
  virtual SIO::CommandType CheckCommandFrame(const UBYTE *CommandFrame,int &datasize,
					     UWORD speed);
  //
  // Acknowledge the command frame. This is called as soon the SIO implementation
  // in the host system tries to receive the acknowledge function from the
  // client. Will return 'A' in case the command frame is accepted. Note that this
  // is only called if CheckCommandFrame indicates already a valid command.
  virtual UBYTE AcknowledgeCommandFrame(const UBYTE *cmdframe,UWORD &delay,UWORD &speed);
  //
  // Read bytes from the device into the system. Returns the number of
  // bytes read.
  virtual UBYTE ReadBuffer(const UBYTE *CommandFrame,UBYTE *buffer,
			   int &datasize,UWORD &delay,UWORD &speed);
  //  
  // Write the indicated data buffer out to the target device.
  // Return 'C' if this worked fine, 'E' on error.
  virtual UBYTE WriteBuffer(const UBYTE *CommandFrame,const UBYTE *buffer,
			    int &datasize,UWORD &delay,UWORD speed);
  //
  // Execute a status-only command that does not read or write any data except
  // the data that came over AUX1 and AUX2
  virtual UBYTE ReadStatus(const UBYTE *CommandFrame,UWORD &delay,
			   UWORD &speed);
  //  
  // After a written command frame, either sent or test the checksum and flush the
  // contents of the buffer out. For block transfer, SIO does this for us. Otherwise,
  // we must do it manually.
  virtual UBYTE FlushBuffer(const UBYTE *CommandFrame,UWORD &delay,
			    UWORD &speed);
  //
  // Other methods imported by the SerialDevice class:
  //
  // ColdStart and WarmStart
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Miscellaneous drive management stuff:
  //
  // Argument parser stuff: Parse off command line args
  virtual void ParseArgs(class ArgParser *args);
  //
  // Status display
  virtual void DisplayStatus(class Monitor *mon);
};
///

///
#endif
