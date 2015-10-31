/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atarisio.cpp,v 1.26 2015/08/15 14:52:35 thor Exp $
 **
 ** In this module: Support for real atari hardware, connected by 
 ** Matthias Reichl's atarisio interface.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "stdio.hpp"
#include "unistd.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "atarisioport.hpp"
#include "atarisio.hpp"
///

/// AtariSIO::AtariSIO
// Constructor of the AtariSIO interface. Initialize buffers.
AtariSIO::AtariSIO(class Machine *mach,const char *name,int id)
  : SerialDevice(mach,name,'1'+id), DriveId(id),
    DoubleDensity(false),
    WriteProtected(false), EnableSIO(false),
    TimeOut(7), FormatTimeOut(60),
    DataFrame(new UBYTE[256])
{ 
  Response = 0;
  ExpectCmdHandshake  = false;
  ExpectDataHandshake = false;
}
///

/// AtariSIO::~AtariSIO
// Cleanup the AtariSIO class
AtariSIO::~AtariSIO(void)
{
  delete[] DataFrame;
}
///

/// AtariSIO::CheckCommandFrame
// Check whether this device accepts the indicated command
// as valid command, and return the command type of it.
// Unfortunately, we cannot communicate directly with the
// diskdrive here already. It also knows what the command
// frame type should be, but SIO needs to know in advance.
SIO::CommandType AtariSIO::CheckCommandFrame(const UBYTE *commandframe,int &datasize,UWORD speed)
{  
  class AtariSIOPort *port = machine->SioPort();
  UWORD            sector  = UWORD(commandframe[2] | (commandframe[3] << 8));
  SIO::CommandType type;
  
  // If we are turned off, signal this. We currently do not emulate speedies.
  if (EnableSIO == false || speed != SIO::Baud19200)
    return SIO::Off;
  //
  // We just check the command here: Get the command type
  switch(commandframe[1]) {
  case 0x3f:  // Read speed byte (extended)
    datasize = 1;
    type     = SIO::ReadCommand;
    break;
  case 0x44:  // set display control byte (extended)
    type     = SIO::StatusCommand; // wierd enough
    break;
  case 0x4b:  // set speed control byte (extended)
    type     = SIO::StatusCommand;
    break;
  case 0x4e:  // read geometry (extended)
    datasize = 12;
    type     = SIO::ReadCommand;
    break;
  case 0x4f:  // write geometry (extended)
    datasize = 12;
    type     = SIO::WriteCommand;
    break;
  case 0x51:  // write back cache (extended)
    type     = SIO::StatusCommand;
    break;
  case 0xd0:
  case 0xd7:  // XF551 fast write
  case 0x70:
  case 0x77:  // Warp Speed fast write
  case 0x50:  // write w/o verify
  case 0x57:  // write with verify 
    // Should we test for write protection?
    datasize = (DoubleDensity && sector > 3)?256:128;
    type     = SIO::WriteCommand;
    break; 
  case 0xa1:
  case 0xa2:
  case 0x21:  // format single density
  case 0x22:  // format enhanced density
    // Should we test for write-protection?
    datasize = DoubleDensity?256:128;
    type     = SIO::FormatCommand;
    break;
  case 0x23:  // Start drive test: 1050 only. Maybe the diags are supported by other drive types as well?
    datasize = 128;
    type     = SIO::WriteCommand;
    break;
  case 0x24:
    datasize = 128;
    type     = SIO::ReadCommand;
    break;
  case 0xd2:  // doubler fast read
  case 0x72:  // Warp fast read
  case 0x52:  // read
    // FIXME: Should we read the sector from the
    // device first to check whether we should answer
    // this by ReadCommand or InvalidCommand?
    datasize = (DoubleDensity && sector > 3)?256:128;
    type     = SIO::ReadCommand;
    break;
  case 0xd3:
  case 0x73:
  case 0x53:  // Read Status
    datasize = 4;
    type     = SIO::ReadCommand;
    break;
  default:
    type     = SIO::InvalidCommand;
    break;
  }
  //
  // Now check whether we are running in direct IO. If so,
  // then transmit the command directly to the device (unless it's an illegal command in first place)
  if (type != SIO::InvalidCommand && port->DirectMode()) {
    // This will pull CMD, transmit the bytes and starts
    // the timing for the SIO transmission. This also automatically
    // attaches a checksum and transmits it along with the command
    // frame.
    port->TransmitCommandFrame(commandframe);
    // Note the size of the data frame in bytes in case we expect one.      
#if CHECK_LEVEL > 0
    if (DataFrameSize > 256) {
      // Huh, how did that happen?
      Throw(OutOfRange,"AtariSIO::CheckCommandFrame","detected internal data frame overflow");
    }
#endif
    switch(type) {
    case SIO::StatusCommand:
      // No data whatsoever is transmitted in status commands.
      DataFrameSize     = 0;
      datasize          = 0;
      break;
    case SIO::ReadCommand:
    case SIO::FormatCommand:
      DataFrameSize     = datasize;
      break;
    case SIO::WriteCommand:
      // This is special because we need to
      // signal SIO that we want single-byte transfer
      DataFrameSize     = datasize;
      datasize          = 0;
      break;
      // Unhandled cases: Shut up the g++
    case SIO::InvalidCommand:
    case SIO::Off:
      break;
    }
    //
    // Keep internal data for generating checksums etc,etc.
    CmdType             = type;
    ChkSum              = 0;
    DataFramePtr        = DataFrame;
    // We do not wait here for the handshaking of the device.
    // Do that later as first step before we continue the data
    // frame.
    ExpectCmdHandshake  = true;
    ExpectDataHandshake = false;
    // Let the emulator run on from here.
  }
  return type;
}
///

/// AtariSIO::ReadStatusBlock
// Read the 815 status block, then adjust the
// internal settings from the returned block.
UBYTE AtariSIO::ReadStatusBlock(const UBYTE *cmdframe,UBYTE *buffer)
{
  UBYTE result;
  
  if ((result = External(false,cmdframe,buffer,12)) == 'C') {
    AdaptDensity(buffer);
    // Return the bytesize of the buffer
    return 'C';
  }
  // Otherwise, return an error.
  return result;
}
///

/// AtariSIO::AdaptDensity
// Interpret a 815 status block and change the density if
// required.
void AtariSIO::AdaptDensity(const UBYTE *buffer)
{
  UWORD sectorsize;
  // The command completed. Now check whether we have a single
  // or a double density disk.
  sectorsize = (UWORD(buffer[6]) << 8) | UWORD(buffer[7]);
  if (sectorsize == 128) {
    DoubleDensity = false;
  } else if (sectorsize == 256) {
    DoubleDensity = true;
  } else {
    // Outch, what do we have here?
    machine->PutWarning("ReadStatusBlock command returned invalid sector size %d\n",sectorsize);
  }
}
///

/// AtariSIO::WriteStatusBlock
// Write out a status block to the device.
// This could possibly change the sector size, so we need
// to get informed about it.
UBYTE AtariSIO::WriteStatusBlock(const UBYTE *cmdframe,const UBYTE *buffer,int size)
{
  UBYTE result;
  UBYTE *buf;
  // The buffer remains in fact constant, but the AtariSIO interface doesn't know
  // this
  buf = TOUBYTE(buffer);
  // First check whether the device accepts this commandframe. If it does,
  // then check for the sector size we wrote into the device.
  result = External(true,cmdframe,buf,size);
  if (result == 'C') {
    AdaptDensity(buffer);
    // Return the bytesize of the buffer
    return 'C';
  }
  // Otherwise, return the error.
  return result;
}
///

/// AtariSIO::AcknowledgeCommandFrame
// Acknowledge the command frame. This is called as soon the SIO implementation
// in the host system tries to receive the acknowledge function from the
// client. Will return 'A' in case the command frame is accepted. Note that this
// is only called if CheckCommandFrame indicates already a valid command.
UBYTE AtariSIO::AcknowledgeCommandFrame(const UBYTE *,UWORD &,UWORD &speed)
{
  class AtariSIOPort *port = machine->SioPort();

  speed = SIO::Baud19200;
  //
  // Operating in direct mode. Read the acknowledge data from the
  // serial device itself.
  if (port->DirectMode()) {
    UBYTE data;
    // User space I/O. First test whether we need to receive a still missing
    // status frame from the device in reaction to the command frame.
    // If so, expect it here and check for correctness.
    if (ExpectCmdHandshake) {
      // Try to read the status from outside. This might return zero
      // if the byte did not yet arrive.
      if (port->ReadDirectByte(data) == 0) {
	// Return with a result = 0 and a data frame length of zero to
	// indicate that no data is (yet) available.
	return 0;
      } else {
	// Otherwise, check whether the command is acceptable. Note
	// that we got the handshake now, one way or another.
	ExpectCmdHandshake = false;
	// We now wait for the data handshake that has to arrive
	// after the command got acklowedged. 
	switch(data) {
	case 'A':
	case 'C': // The Atari Os SIO also accepts 'C' here.
	  // Acknowledged. Expect a data frame now.
	  ExpectDataHandshake = true;
	  return data;
	  break;
	default:
	  // Error. Signal the error up to the calling chain and abort
	  // the command.
	  return data;
	}
      }
    } else {
      machine->PutWarning("AtariSIO communication is out of sync.\n"
			  "The emulator requested a command acknowledge, though\n"
			  "AtariSIO did not expect to deliver one.");
      // Signal a NAK.
      return 'N';
    }
  } else {
    // This is the kernel-based AtariSIO communication. At this time, acknowledge the command even
    // though we possibly do not even know whether we can... *sigh*
    return 'A';
  }
}
///

/// AtariSIO::ReadBuffer
// Fill a buffer by a read command, return the amount of
// data read back (in bytes), not counting the checksum
// byte which is computed for us by the SIO.
// Prepare the indicated command. On read, read the buffer. On
// write, just check whether the target is write-able. Returns
// the size of the buffer (= one sector). On error, return 0.
UBYTE AtariSIO::ReadBuffer(const UBYTE *commandframe,UBYTE *buffer,int &len,UWORD &,UWORD &speed)
{ 
  class AtariSIOPort *port = machine->SioPort();
  UWORD sector = UWORD(commandframe[2] | (commandframe[3] << 8));
  UWORD sectorsize;

  speed = SIO::Baud19200;

  if (port->DirectMode()) {
    UBYTE data;
    len = 0; // default return: No data available.
    // User space I/O. First test whether we need to receive a still missing
    // status frame from the device in reaction to the command frame.
    // If so, expect it here and check for correctness.
    if (ExpectCmdHandshake) {
      machine->PutWarning("AtariSIO communication is out of sync.\n"
			  "The emulator requested a data acknowledge, though\n"
			  "AtariSIO did not receive a command.");
      // Signal a NAK.
      return 'N';
    }
    //
    // This is now the data handshake.
    if (ExpectDataHandshake) {
      // Check whether we already have a handshake returned from the
      // external device.
      if (port->ReadDirectByte(data) == 0) {
	// Return with result == 0 to tell SIO that we need to wait
	// a bit longer.
	return 0;
      }
      //
      // One way or another, the data handshake is here.
      ExpectDataHandshake = false;
      // Check for the kind of result. Might be an error, or something
      // like it. Interestingly, Atari devices expect to send the
      // data frame, even a dummy data frame, if an error is signalled.
      // Thus, we cannot simply exit here.
      Response = data;
    }
    // Try to read data from the port. If nothing's on, return
    // SIO that Pokey shall wait a bit longer.
    while(DataFrameSize) {
      // Perform plain data reading until we got all of them.
      if (port->ReadDirectByte(data) == 0) {
	return 0;
      }
      // Did a plain data read. Add up the checksum and return the byte.
      if (int(ChkSum) + data >= 256)
	ChkSum++;
      ChkSum         += data;
      DataFrameSize--;
      // Insert the data also into our internal buffer
      *buffer++       = data;
      *DataFramePtr++ = data;
      // Read one byte more. Note that len was initialized to zero.
      len++;
    }
    // Check whether we still have the checksum in the buffer.
    if (port->ReadDirectByte(data) == 0) {
      // No.
      return 0;
    }
    // Last byte of the transmission; this is the checksum. Test for correctness
    // and generate an error condition by hand if not. This is required as
    // SIO does not test the checksum, but rather generates one.
    if (ChkSum != data) {
      return 'E';
    }
    // Here the command completed. For some commands, we also perform
    // an internal status change depending on whether the command
    // returned successfully or not.
    switch(commandframe[1]) {
    case 0x4e: // Read 815 Status block, adapt density
      AdaptDensity(DataFrame);
      break;
    }
    return Response; // Command completed, or maybe not.
  } else {
    // Kernel I/O
    switch (commandframe[1]) {
    case 0x3f: // Read Speed Byte
      // This is a one-byte command
      len = 1;
      return External(false,commandframe,buffer,1);
    case 0x4e: // Read Status
      // This also has to set the new density control
      len = 12;
      return ReadStatusBlock(commandframe,buffer);
    case 0x52: // Read (read command)  
    case 0xd2:
    case 0x72:
      // This depends on the density setting and on the
      // sector. The first three sectors of a HD disk are
      // still 128 bytes.
      sectorsize = (DoubleDensity && sector > 3)?256:128;
      len = sectorsize;
      return External(false,commandframe,buffer,sectorsize);
    case 0x53: // Status (read command)
    case 0xd3:
    case 0x73:
      // The status is four bytes LONG if it exists.
      len = 4;
      return External(false,commandframe,buffer,4);
    case 0x21: // single density format
    case 0x22: // double density format
    case 0xa1:
    case 0xa2:
      sectorsize = DoubleDensity?256:128;
      len = sectorsize;
      return External(false,commandframe,buffer,sectorsize);
    }
  }
  
  machine->PutWarning("Unknown command frame: %02x %02x %02x %02x\n",
		      commandframe[0],commandframe[1],commandframe[2],commandframe[3]);
  return 'N';
}
///

/// AtariSIO::WriteBuffer
// Write the indicated data buffer out to the target device.
// Return 'C' if this worked fine, 'E' on error.
UBYTE AtariSIO::WriteBuffer(const UBYTE *cmdframe,const UBYTE *buffer,int &size,UWORD &,UWORD)
{      
  class AtariSIOPort *port = machine->SioPort();
  // Check whether we are performing direct (user space) I/O. If so, then
  // run it here directly.
  if (port->DirectMode()) {
    UBYTE data;
    int inputsize = size;
    size = 0; // Default return: No data written.
    // User space I/O. First test whether we still need to read the
    // command frame handshaking.
    if (ExpectCmdHandshake) { 
      machine->PutWarning("AtariSIO communication is out of sync.\n"
			  "The emulator requested a data acknowledge, though\n"
			  "AtariSIO did not receive a command.");
      // Signal a NAK.
      return 'N';
    }
    //
    // Now transmit the data frame contents byte by byte.
    while (inputsize && DataFrameSize) {
      // There is still a dataframe to transmit. Do so now.
      data = *buffer++;
      if (int(ChkSum) + data >= 256)
	ChkSum++;
      ChkSum        += data;
      *DataFramePtr += data;
      DataFrameSize--;
      inputsize--;
      port->WriteDirectByte(data);
      //
      // Wrote now a single byte, signal to the caller.
      size++;
      // Test whether we just sent the last data byte. If so,
      // test for the checksum we just generated.
      if (DataFrameSize == 0) {
	// Now write the generated checksum as an additional
	// byte.
	port->WriteDirectByte(ChkSum);
	// We are completed. The real data
	// status is generated/read by the Flush method.
	// The real device acknowledge is checked in the Flush
	// method.
	return 'A';
      }
    }
    // No status return. Come back when done to write the next byte.
    return 0;
  } else { 
    UBYTE *buf;
    // The buffer remains constant, but the AtariSIO interface doesn't 
    // know it.
    buf = TOUBYTE(buffer);
    // kernel I/O
    switch (cmdframe[1]) { 
    case 0x4f:  // Write Status Block
      // This is a separate function call since we need to adjust the double
      // density byte as well.
      return WriteStatusBlock(cmdframe,buffer,size);
    case 0x70:
    case 0x77:
    case 0xd0:
    case 0xd7:
    case 0x50: 
    case 0x57: // Write with and without verify
      // We check here special sector counts for the Speedy CPU RAM
      // access.
      // The sector size comes in from outside.
      return External(true,cmdframe,buf,size);
    }
    return 'E';
  }
}
///

/// AtariSIO::FlushBuffer
// After a written command frame, either sent or test the checksum and flush the
// contents of the buffer out. For block transfer, SIO does this for us. Otherwise,
// we must do it manually.
UBYTE AtariSIO::FlushBuffer(const UBYTE *commandframe,UWORD &,UWORD &speed)
{
  class AtariSIOPort *port = machine->SioPort();
  
  speed = SIO::Baud19200;
  
  if (port->DirectMode()) {
    UBYTE data;
    // If we just performed a write command, expect the device to answer
    // back with a data frame status and a command status.
    // Problem is that we already acknowledged the data even though the 
    // device did not. Thus, first attempt to receive the device response
    // for the data frame.
    if (CmdType == SIO::WriteCommand) {
      if (ExpectDataHandshake) {
	// Run the final command handshake acknowledge, then terminate the
	// command.
	if (port->ReadDirectByte(data)) {
	  // Test for internal status changes due to the written command. This
	  // goes for density changes.
	  if (data == 'C' || data == 'A') {
	    // Data transmission was successful. Now analyze the command.
	    switch(commandframe[1]) {
	    case 0x4f:  // Write Status Block
	      AdaptDensity(DataFrame);
	      break;
	    }
	    //
	    // Keep the data frame response so we can fixup the
	    // final device response if necessary.
	    Response = data;
	  }
	  // Note that this does not fix the transmission. It only signals that the device
	  // accepted the data. Thus, signal a "please come back here later" to fetch
	  // the final response code.
	  ExpectDataHandshake = false;
	}
	//
	// In either case, request the caller to come back to receive the final data
	// write phase response.
	return 0;
      } else {
	// Read the final data response. This is sent by the device as soon as the operation
	// is completed.
	if (port->ReadDirectByte(data)) {
	  // If this is a "completed", but the data was not accepted, change the final response.
	  if (data == 'C' || data == 'A') {
	    if (Response != 'A' && Response != 'C')
	      data = Response;
	  }
	  //
	  // Ok, done with it.
	  return data;
	}
	//
	// Come back later.
	return 0;
      }
    }
    machine->PutWarning("Unexpected data frame flush on command frame %02x %02x %02x %02x\n",
			commandframe[0],commandframe[1],commandframe[2],commandframe[3]);
    return 'E';
  }
  // All has been handled by the kernel already. Sent the same result as
  // all other block devices.
  return 'C';
}
///

/// AtariSIO::ReadStatus
// Execute a status-only command that does not read or write any data except
// the data that came over AUX1 and AUX2
UBYTE AtariSIO::ReadStatus(const UBYTE *,UWORD &,UWORD &speed)
{ 
  class AtariSIOPort *port = machine->SioPort();
  // Check whether we are direct-IO here. If so, then we have to
  // wait at least for the command handshake to return.
  speed = SIO::Baud19200;
  // Check whether we are performing direct (user space) I/O. If so, then
  // run it here directly.
  if (port->DirectMode()) {
    UBYTE data;
    // User space I/O. First test whether we still need to read the
    // command frame handshaking. This should not happen since
    // AcknowledgeCommandFrame() should have been called.
    if (ExpectCmdHandshake) {
      machine->PutWarning("AtariSIO communication is out of sync.\n"
			  "The emulator requested a data acknowledge, though\n"
			  "AtariSIO did not receive a command.");
      // Signal a NAK.
      return 'N';
    }
    //
    // Now check whether still have to wait for
    // the data handshake
    if (ExpectDataHandshake) {
      if (port->ReadDirectByte(data)) {
	// Received it, now return it.
	ExpectDataHandshake = false;
	// Return the command status now.
	return data;
      }
    }
    //
    // Signal the caller that we need more time.
    return 0;
  }
  // The kernel interface does not support status only commands, so failing
  // is the only alternative here.
  return 'N';
}
///

/// AtariSIO::External
// Transmit a command to an external device by means
// of the kernel interface. Returns the result character
// of the external device.
// The first boolean describes the data direction. Unfortunately,
// AtariSIO knows only IN or OUT, not STATUS.
// The second array is the command frame,
// the next the source or the target buffer, the last the byte
// size.
char AtariSIO::External(bool writetodevice,const UBYTE *commandframe,UBYTE *buffer,int size)
{
  class AtariSIOPort *port = machine->SioPort();
  UBYTE command = commandframe[1];
  UBYTE timeout;
  //
  if (EnableSIO) {
    // Analyze the timeout for the command now.
    switch(command) {
    case 0x21:
    case 0x22:
    case 0xa1:
    case 0xa2:
      // Various format commands. These take the longer timeout
      timeout   = FormatTimeOut;
      break;
    default:
      // Regular commands.
      timeout   = TimeOut;
    }
    return port->External(writetodevice,commandframe,buffer,size,timeout);
  } else {
    // No command acknowledge
    return 0;
  }
}
///

/// AtariSIO::WarmStart
// Run a warmstart of our local drive. Doesn't do much.
// However, since the external drive is not reset here,
// we do not reset the double density and write protection flags.
void AtariSIO::WarmStart(void)
{    
  class AtariSIOPort *port = machine->SioPort();
  // In case we have a SIO Port, also reset it.
  // This is done here since SIO may reset us manually on
  // a command frame error.
  if (port)
    port->WarmStart();
  // Reset the status of the DirectIO state machine.
  ExpectCmdHandshake  = false;
  ExpectDataHandshake = false;
  Response            = 0;
}
///

/// AtariSIO::ColdStart
// Run a coldstart. This is currently the same as the
// warmstart, currently.
void AtariSIO::ColdStart(void)
{
  WarmStart();
}
///

/// AtariSIO::ParseArgs
// Parse off command line arguments for this class
void AtariSIO::ParseArgs(class ArgParser *args)
{
  char enableoption[32],protectoption[32];
  char timeoutoption[32];
  char formatoption[32];
  LONG timeout  = TimeOut;
  LONG formtime = FormatTimeOut;
  
  // generate the appropriate options now
  sprintf(enableoption,"SioEnable.%d",DriveId+1);
  sprintf(protectoption,"SioProtect.%d",DriveId+1);
  sprintf(timeoutoption,"SioTimeOut.%d",DriveId+1);
  sprintf(formatoption,"SioFormatTimeOut.%d",DriveId+1);
  
  if (DriveId == 0)
    args->DefineTitle("AtariSIO");
  args->DefineBool(enableoption,"enable the external drive",EnableSIO);
  args->DefineBool(protectoption,"inhibit writes to the external drive",WriteProtected);  
  args->DefineLong(timeoutoption,"default timeout in seconds",1,30,timeout);
  args->DefineLong(formatoption,"timeout for disk format commands in seconds",10,120,formtime);
  //
  // Convert the data into the internal representation.
  TimeOut       = UBYTE(timeout);
  FormatTimeOut = UBYTE(formtime);
}
///

/// AtariSIO::DisplayStatus
// Print the status of the SIO class to the monitor
// for debugging purposes.
void AtariSIO::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("AtariSIO #%d status:\n"
		   "\tDrive enabled      : %s\n"
		   "\tWrite protection   : %s\n"
		   "\tTimeOut            : %ds\n"
		   "\tFormat TimeOut     : %ds\n\n",
		   DriveId+1,
		   (EnableSIO)?("yes"):("no"),
		   (WriteProtected)?("write protected"):("read only"),
		   TimeOut,FormatTimeOut
		   );
}
///

