/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sio.cpp,v 1.68 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Generic emulation core for all kinds of serial hardware
 ** like printers or disk drives.
 **********************************************************************************/

/// Includes
#include "sio.hpp"
#include "monitor.hpp"
#include "machine.hpp"
#include "mmu.hpp"
#include "diskdrive.hpp"
#include "printer.hpp"
#include "pokey.hpp"
#include "display.hpp"
#include "timer.hpp"
#include "new.hpp"
///

/// SIO::SIO
// Constructor of the SIO class
SIO::SIO(class Machine *mach)
  : Chip(mach,"SIO"), 
    DataFrame(NULL), DataFrameSize(0)
{
  pokey              = NULL;
  SerIn_Cmd_Delay    = 50;
  Write_Done_Delay   = 50;
  Read_Done_Delay    = 50;
  Read_Deliver_Delay = 2;
  Format_Done_Delay  = 400;
  MotorEnabled       = false;
  HaveWarned         = false;
  ActiveDevice       = NULL;
}
///

/// SIO::~SIO
SIO::~SIO(void)
{
  class SerialDevice *ser;
  //
  // Now dispose all serial devices within here.
  while ((ser = Devices.RemHead())) {
    delete ser;
  }
  delete[] DataFrame;
}
///

/// SIO::ReallocDataFrame
// Ensure the dataframe is large enough for the requested
// number of bytes, realloc otherwise.
void SIO::ReallocDataFrame(int framesize)
{
  // Add one byte for the checksum, one for
  // the single byte transfer that is indicated
  // by a block size of zero.
  framesize += 2;
  // Enlarge the data frame buffer if required.
  if (framesize > DataFrameSize) {
    delete[] DataFrame;
    DataFrame     = NULL;
    DataFrame     = new UBYTE[framesize];
    // Shut up valgrind
    memset(DataFrame,0,framesize);
    DataFrameSize = framesize;
  }
}
///

/// SIO::RegisterDevice
// Register a serial device for handling with SIO.
// This will also dispose the device when we're done.
void SIO::RegisterDevice(class SerialDevice *device)
{
  Devices.AddTail(device);
}
///

/// SIO::ChkSum
// Compute a checksum over a sequence of bytes in the SIO way.
UBYTE SIO::ChkSum(const UBYTE *buffer,int bytes)
{
  int sum = 0;
 
  while(bytes) {
    sum += *buffer++;
    if (sum > 255)
      sum -= 255;
    bytes--;
  }

  return UBYTE(sum);
}
///

/// SIO::FindDevice
// Given a command frame, identify the device responsible to handle
// this frame, or NULL if the device is not mounted.
class SerialDevice *SIO::FindDevice(UBYTE commandframe[4])
{
  class SerialDevice *dev;

  for(dev = Devices.First();dev;dev = dev->NextOf()) {
    if (dev->HandlesFrame(commandframe))
      return dev;
  }
  return NULL;
}
///

/// SIO::ColdStart
void SIO::ColdStart(void)
{
  // Grab interfacing chips now
  pokey    = machine->Pokey();
  //
  // Reset the state machine
  SIOState = NoState;
  //
  WarmStart();
}
///

/// SIO::WarmStart
// Signal a warm start to SIO and all its devices
void SIO::WarmStart(void)
{
  // Reset the state machine
  SIOState          = NoState;
  MotorEnabled      = false;
  HaveWarned        = false;
  ActiveDevice      = NULL;
}
///

/// SIO::WriteByte
// Transmit a byte from pokey to the serial device.
void SIO::WriteByte(UBYTE byte)
{
  UWORD delay;

  // Check which state we are in
  switch(SIOState) {
  case CmdState: 
    // We are currently in a command frame, the serial device
    // receives the SIO command.
    if (CommandFrameIdx < ExpectedBytes) {
      // There are still some command bytes to be transfered
      CommandFrame[CommandFrameIdx++] = byte;
      if (CommandFrameIdx >= ExpectedBytes) {
	// Transfer of command frame is complete. Run the
	// corresponding device to check for its state.
	if ((ActiveDevice = FindDevice(CommandFrame))) {
	  UBYTE sum;
	  // Check the checksum
	  sum = ChkSum(CommandFrame,ExpectedBytes - 1);
	  if (sum == CommandFrame[ExpectedBytes - 1]) {
	    // Get the current command type and the expected size
	    // of the data frame. For first, reset the data frame
	    // size to zero.
	    DataFrameLength = 0;
	    CmdType = ActiveDevice->CheckCommandFrame(CommandFrame,DataFrameLength,
						      pokey->SerialTransmitSpeed());
	    if (CmdType != Off) {	      
	      // Check whether we have enough data buffer here; if not,
	      // enlarge it. Note that we need one additional byte
	      // for the checksum	    
	      ReallocDataFrame(DataFrameLength);
	      // Reset the checksum now, and the index into the data frame.
	      CurrentSum   = 0;
	      DataFrameIdx = 0;
	      // If the device is turned on, ensure that we
	      // generate an answer back message. This must be
	      // 'A' if the command got acknowledged.
	      // For direct IO transmissions, we pre-generate the
	      // result here and get the "real" device result later.
	      // This is not very nice, but such is life. It helps to
	      // keep the transmission fast.
	      if (CmdType == InvalidCommand) {
		// Reject the command immediately.
		CommandStatus = 'N';
		// This is just the command frame acknlowedge, not yet the
		// command completion byte
		pokey->SignalSerialBytes(&CommandStatus,1,UWORD(SerIn_Cmd_Delay),
					 pokey->SerialReceiveSpeed());
		// Done with it.
		SIOState = NoState;
	      } else {
		// Otherwise accept the command for now and wait until the
		// atari asks again.
		CommandStatus = 'A';
		// Just tell pokey that something is waiting.
		pokey->SignalSerialBytes(NULL,0,UWORD(SerIn_Cmd_Delay),
					 pokey->SerialReceiveSpeed());
		// Now accept the second byte to be returned from the device.
		// This indicates whether the command completes (for read
		// commands)
		SIOState = AcknowledgeState;
	      }
	      // Done with it.
	      return;
	    }
	  } else {
	    // According to the SIO specs, devices should not react on command frames
	    // with invalid checksums.
	    // Command frame is just nonsense
	    machine->PutWarning("Checksum of SIO command frame is invalid, ignoring it.\n");
	  }
	}
	// Device does not exist or is turned off, or the command
	// was invalid. In the latter case, the DeviceNAK has been
	// send back already.
	SIOState = NoState; // just don't react. Nobody at home!
	break;
      }
    } else {
      // Command frame is just nonsense
      machine->PutWarning("Received invalid command frame at SIO.\n");
      SIOState = NoState;
    }
    break;
  case WriteState: 
    // Feed our input data buffer (write to device)
    // Actually, the delay is never needed here.
    delay = UWORD(Write_Done_Delay);
    // First case is the case that we expected the bytes and hence
    // run a block transfer where we first buffer the bytes
    // and write them in one go.
    if (ExpectedBytes) {
      if (DataFrameIdx < ExpectedBytes) {
	// Still have to read some bytes (i.e. expect writes)
	DataFrame[DataFrameIdx++] = byte;
	if (DataFrameIdx >= ExpectedBytes) {
	  UBYTE sum;
	  // Frame is complete now. Generate a checksum and
	  // check whether it is fine.
	  sum = ChkSum(DataFrame,ExpectedBytes - 1);
	  if (sum == DataFrame[ExpectedBytes - 1]) {
	    int len = ExpectedBytes - 1;
	    UBYTE result;
	    // The checksum is just fine. Now check whether
	    // the serial device writes the buffer fine.
	    // We do not include the checksum. 
	    result = ActiveDevice->WriteBuffer(CommandFrame,DataFrame,len,delay,
					       pokey->SerialTransmitSpeed());
	    if (result) {
	      // Switch to the flush frame to collect the final result
	      // after writing the data out.
	      // Bummer! If WriteBuffer() already performed the command,
	      // then this should be the final response and not the intermediate
	      // response.
	      StatusFrame[0] = result;
	      SIOState       = FlushState;
	      // Ask pokey to come back later.
	      pokey->SignalSerialBytes(NULL,0);
	    } else {
	      // No reaction otherwise. Yuck.
	      SIOState       = NoState;
	    }
	  } else {
	    // A checksum error. Signal this directly, don't bother the device.
	    StatusFrame[0] = 'N'; // Must be a NAK, see SIO documentation
	    // The SIO docs also state that only the NAK shall be send, but this is not
	    // how the Os works. It always expects a second indicator here.
	    StatusFrame[1] = 'E'; 
	    pokey->SignalSerialBytes(StatusFrame,1,0,pokey->SerialReceiveSpeed());
	    // Come again to collect the next error.
	    SIOState       = DoneState;
	  }
	}
      } else {
	machine->PutWarning("Received overLONG data frame.\n");
	SIOState = NoState;
      }
    } else {
      UBYTE result;
      int len;
      // Second case. We do not expect the bytes, but rather expect the device
      // to expect them. Uhm. 2nd try: Single byte transfer. The device must keep
      // checksums for us.
      // Push the data into the data buffer now.
      DataFrame[DataFrameIdx++] = byte;	
      // Update the checksum now.
      if (int(CurrentSum) + byte >= 256)
	CurrentSum++;
      CurrentSum += byte;
      len         = DataFrameIdx;
      // Check whether the device can write this (and the remaining buffer) out.
      result = ActiveDevice->WriteBuffer(CommandFrame,DataFrame,len,delay,
					 pokey->SerialTransmitSpeed());
      // Remove the written bytes from the buffer.
      if (len > 0) {
	if (len < DataFrameIdx)
	  memmove(DataFrame,DataFrame+len,DataFrameIdx - len);
	DataFrameIdx -= len;
      }
      // If we got a non-zero result code, then the data has been sent over
      // or an error has been collected.
      if (result) {
	// Insert the status into the status buffer, go to the flush frame.
	// This frame also expects the checksum there.
	StatusFrame[1] = result;
	SIOState       = FlushState;
      }
    }
    break;
  case FlushState:
    // If we enter here, then from a single-byte transfer of the checksum.
    // Test whether the checksum is consistent. If not, force-generate an
    // error condition on the already collected status. This has the draw-back
    // that the serial device interface then already wrote out its own checksum,
    // hence it is actually too late to fix this.
    if (CurrentSum != byte) {
      // The troublesome case.
      // Must sent a NAK according to the rules.
      StatusFrame[0] = 'N';
      // Still run into the read-byte case to have this status read back
      // into the emulated Atari.
    }
    // Tell pokey that it can pick up a byte later.
    pokey->SignalSerialBytes(NULL,0);
    break;
  default:
    // Otherwise claim that something unexpected happened
    // NOTE: Since the XL Os writes here a 0xff on reset, we do not
    // warn in this special case. Yuck.
    if (byte != 0xff) {
      if (!HaveWarned) {
	machine->PutWarning("Unexpected SIO data %02x received\n",byte);
	HaveWarned = true;
      }
    }
  }
}
///

/// SIO::RequestInput
// Request more input bytes from SIO by a running
// command.
void SIO::RequestInput(void)
{
  UBYTE result;
  UWORD delay;
  int bytes;
  int sum;
  
  switch(SIOState) {
  case AcknowledgeState:
    // Check whether the command got accepted by the device. Depending on the command
    // type, we either need to call the status collector, or the command acknowledge.
    delay         = SerIn_Cmd_Delay;
    InputSpeed    = pokey->SerialReceiveSpeed();
    switch(CmdType) {
    case ReadCommand:
    case FormatCommand:
    case WriteCommand:
      result = ActiveDevice->AcknowledgeCommandFrame(CommandFrame,delay,InputSpeed);
      if (result) {
	if (result == 'A' || result == 'C')
	  StatusFrame[0] = 'A';
	pokey->SignalSerialBytes(StatusFrame,1,delay,InputSpeed);
	if (StatusFrame[0] == 'A') {
	  SIOState = StatusState;
	} else {
	  SIOState = NoState;
	}
      } else {
	// Otherwise ask pokey to come back later.
	pokey->SignalSerialBytes(NULL,0);
      }
      break;
    case StatusCommand:
      // This is similar to read except that zero bytes are transfered
      // and the result is immediate.
      result = ActiveDevice->ReadStatus(CommandFrame,delay,InputSpeed);
      if (result) {
	StatusFrame[0] = result;
	pokey->SignalSerialBytes(StatusFrame,1,delay,InputSpeed);
	// Done with it.
	SIOState = NoState;
      } else {
	// Otherwise ask pokey to come back later.
	pokey->SignalSerialBytes(NULL,0);
      }
      break;
    case InvalidCommand:
      // Transmit the error immediately.
      // The code should actually not enter here.
      StatusFrame[0] = 'N';
      pokey->SignalSerialBytes(StatusFrame,1,delay,InputSpeed);
      // We are done with the handling
      SIOState       = NoState;
      break;
    case Off:
      // The device does not want to react at all. Do not answer, to not
      // transmit a status
      SIOState = NoState;
      break;
    }
    break;
  case StatusState:
    // The command was accepted by the device, and Pokey read
    // this acceptance fine. Now execute this command by the 
    // corresponding device.
    switch (CmdType) {
    case ReadCommand:
    case FormatCommand:
      // Try to read the first byte. If we get it, switch to the read
      // frame and read off the data bytes. Otherwise, wait until
      // we get something, then signal the result.    
      bytes         = DataFrameLength; // Size of the expected data buffer.
      ExpectedBytes = DataFrameLength;
      InputSpeed    = pokey->SerialReceiveSpeed();
      if (CmdType == FormatCommand) {
	delay       = UWORD(Format_Done_Delay);
      } else {
	delay       = UWORD(Read_Done_Delay);
      }
      result        = ActiveDevice->ReadBuffer(CommandFrame,DataFrame,bytes,delay,InputSpeed);
      // Check whether we already got any bytes in return.
      if (bytes) {
	// Yes, we did. Integrate them into our data frame buffer, 
	// advance the fill-in position and start with SIO
	// checksumming
#if CHECK_LEVEL > 0
	// In case the device read more bytes than requested, something icky
	// happened.
	if (bytes > DataFrameLength)
	  Throw(OutOfRange,"SIO::RequestInput","serial device read more data than requested");
#endif
	// Advance already the data position by the number of read bytes. All later
	// reads are performed in the "ReadFrame" case below.
	ExpectedBytes   -= bytes;
	SIOState         = ReadState;
	DataFrameIdx     = bytes;	
	CurrentSum       = ChkSum(DataFrame,bytes);
	// Read some data from the device. Generate the answer of the
	// command frame here.
	if (result) {
	  StatusFrame[0] = result;
	} else {
	  // Otherwise, signal the completion for the driver
	  StatusFrame[0] = 'C';
	}
	// Mis-use status frame to keep the information whether
	// the current command already completed with this go, or whether
	// we need to come back. If non-zero, the call completed.
	StatusFrame[1]   = result;
	pokey->SignalSerialBytes(StatusFrame,1,delay,InputSpeed);
      } else if (result) {
	// All done, error or completed: We got a non-zero status from the
	// device.
	ExpectedBytes    = 0;
	StatusFrame[0]   = result;
	// Here we just got a result code and no
	// data bytes at all. Abort the frame.
	SIOState                  = NoState;
	pokey->SignalSerialBytes(StatusFrame,1,delay,InputSpeed);
      } else {
	// Otherwise, ask pokey to wait a bit longer and call back later.
	pokey->SignalSerialBytes(NULL,0);
      }
      break;
    case WriteCommand:
      // Just switch into the writing frame, do not send any bytes
      // back right here. Pokey calls us here as soon as its
      // receive buffer becomes empty, even if the Os SIO routine
      // does not try to read SerIn.
      if (DataFrameLength) {
	// Asynchronous completion. Expect one additional byte
	// for the checksum.
	ExpectedBytes = DataFrameLength + 1;
      } else {
	// Synchronous single byte transfer.
	ExpectedBytes = 0;
      }
      // Do not signal anything but switch to the write frame right
      // away.
      SIOState = WriteState;
      break;
    case InvalidCommand:
    case StatusCommand:
    case Off:
      // Code should not go here.
      SIOState = NoState;
      break;
    }
    break;
  case ReadState:
    // We enter here to read data from the device into pokey,
    // after the status read completed. For that, first
    // check whether we have anything in our buffer. If so,
    // checksum and transmit this part. Otherwise, read new
    // data into this buffer.
    //
    // No additional delay from the floppy
    delay            = Read_Deliver_Delay;
    if (DataFrameIdx == 0) {
      // Here we have vacant space in this buffer. Refill it
      // before we continue unless we are already completed.
      bytes          = ExpectedBytes; // bytes we still have to transmit
      if (StatusFrame[1]) {
	// The initial read completed, do not try to read again.
	result       = StatusFrame[1];
      } else {
	InputSpeed   = pokey->SerialReceiveSpeed();
	result       = ActiveDevice->ReadBuffer(CommandFrame,DataFrame,bytes,delay,InputSpeed);
	// If we collected any bytes, add up the checksum.
	sum          = ChkSum(DataFrame,bytes);
	sum         += CurrentSum;
	if (sum     >= 256) sum++;
	CurrentSum   = UBYTE(sum);
      }
      // Does the frame end here? If so, fill in the checksum and
      // return.
      if (result) {
	DataFrame[bytes++] = CurrentSum;
	// Terminate the transmission now.
	SIOState           = NoState;
      }
      // Remember how much we have in the buffer and thus how much we
      // need to forward to pokey.
      DataFrameIdx   = bytes;
      ExpectedBytes -= bytes;
    }
    // Transmit now the bytes in this buffer.
    pokey->SignalSerialBytes(DataFrame,DataFrameIdx,delay,InputSpeed);
    // Now we have zero data available in the buffer, need to
    // refill on the next go.
    DataFrameIdx = 0;
    break;
  case FlushState:
    // A write command returned, we are now expected to deliver
    // the result code. This is the primary result from the
    // data transmission phase, not yet the command return.
    // Check whether flushing out the buffer was succesful. 
    InputSpeed = pokey->SerialReceiveSpeed();
    result     = ActiveDevice->FlushBuffer(CommandFrame,delay,InputSpeed);
    if (result) {
      // Got a result. Test whether it is valid or not. If not,
      // insert this as immediate result and return. 
      if (result == 'E' || result == 'N' || StatusFrame[0] == 'N') {
	// Device NAK? Then only one result, namely this one.
	// The data transmission was erraneously (checksum!)
	if (StatusFrame[0] == 'N')
	  result = 'N'; // Deliver a NAK if the data was not received correctly.
	StatusFrame[1]   = result;
	pokey->SignalSerialBytes(StatusFrame,1,UWORD(SerIn_Cmd_Delay),InputSpeed); 
	SIOState         = DoneState;
      } else {
	// Otherwise, the data transmission must have been fine and we
	// just insert the result code of the command execution.
	// If the Atari++ device returned an 'E', make this the result of the
	// final phase and not of the data acknowledge phase. Also translate
	// the completed into an acknowledge.
	if (StatusFrame[0] == 'E') {
	  result         = 'E';
	  StatusFrame[0] = 'A';
	} else if (StatusFrame[0] == 'C') {
	  StatusFrame[0] = 'A';
	}
	// StatusFrame[1] will be fixed up later.
	StatusFrame[1] = result;
	// Signal the result of the data phase.
	pokey->SignalSerialBytes(StatusFrame,1,UWORD(SerIn_Cmd_Delay),InputSpeed);
	SIOState       = DoneState;
      }
    } else {
      // Tell pokey to wait a bit longer as the device is not yet ready.
      pokey->SignalSerialBytes(NULL,0);
    }
    break;
  case DoneState:
    // The final transmission. Also fix up the code. Note that for Atari++ internal
    // devices, StatusFrame[0] is already the final result and Flush does only
    // return 'C', thus make StatusFrame[1] conform.
    if (StatusFrame[1] == 'A')
      StatusFrame[1] = 'C'; // completed.
    if (StatusFrame[1] == 'N' || (StatusFrame[0] != 'A' && StatusFrame[0] != 'C'))
      StatusFrame[1] = 'E'; // error
    pokey->SignalSerialBytes(StatusFrame+1,1,UWORD(Write_Done_Delay),InputSpeed);
    // We are done with the command, return to the initial state
    SIOState       = NoState;
    break;
  case NoState:
    // Someone requested serial data, but there's currently nothing
    // pending.
    break;
  default:
    // Unexpected frame here
    machine->PutWarning("SIO::RequestInput got stuck at unknown request state.\n");
    break;
  }
}
///

/// SIO::SetCommandLine
// Toggle the command frame on/off:
// This enables or disables the COMMAND
// line on the serial port to indicate that
// a command frame is on its way.
void SIO::SetCommandLine(bool onoff)
{
  if (onoff) {
    // Entering a command frame now. If we are already in a command frame,
    // ignore me.
    HaveWarned   = false;
    if (SIOState == CmdState)
      return;
    if (SIOState != NoState) {
      // Ooops, enabled COMMAND within an active frame?
      // To resync with the device, run it thru a warmstart.
      if (ActiveDevice) {
	ActiveDevice->WarmStart();
	SIOState = NoState;
      }
      machine->PutWarning("Enabled SIO CMD line within an active frame.\n");
    }
    CommandFrameIdx = 0;
    DataFrameIdx    = 0;
    // Expect a SIO command frame here.
    ExpectedBytes   = 5;
    SIOState        = CmdState;
    // Enforce serial transfer resync here. This is strictly speaking never
    // required should every serial transfer work well.
    pokey->SignalCommandFrame();
    machine->Display()->SetLED(true);
  } else {
    // Disabling the command frame.
    machine->Display()->SetLED(false);
    //
    if (SIOState != AcknowledgeState && SIOState != NoState) {
      if (SIOState == CmdState && CommandFrameIdx != 0) {
	machine->PutWarning("Command frame unfinished.\n");
      }
      SIOState      = NoState;
    }
    CommandFrameIdx = 0;
  }
}
///

/// SIO::WaitForSerialDevice
// Private to the above: Delay for the given timer until the
// serial device reacts, and run thru the VBI. Returns true in
// case we must abort the operation.
bool SIO::WaitForSerialDevice(class Timer *time,ULONG &timecount)
{
  // Test whether there is any time left we can wait. If not,
  // abort with timeout.
  if (timecount == 0)
    return true;
  //
  // Test whether the user wants interaction. If so, abort
  // too.
  if (machine->ColdReset())
    return true;
  if (machine->Reset())
    return true;
  if (machine->LaunchMonitor())
    return true;
  if (machine->LaunchMenu())
    return true;
  if (machine->Display()->MenuVerify())
    return true;
  //
  // Ok, no user events. Trigger the VBI
  // for the refresh.
  machine->VBI(time,false,true);
  time->TriggerNextEvent();
  timecount--;
  return false;
}
///

/// SIO::RunSIOCommand
// Bypass the serial overhead for the SIO patch and issues the
// command directly. It returns a status
// indicator similar to the ROM SIO call.
UBYTE SIO::RunSIOCommand(UBYTE device,UBYTE unit,UBYTE command,ADR mem,
			 UWORD size,UWORD aux,UBYTE timeoutsecs)
{
  class AdrSpace *ram;
  class SerialDevice *ser;
  UBYTE cmdframe[4];
  UBYTE result,*buffer;
  CommandType cmdtype;
  class Timer timeout; // a 10ms timer for display refresh
  ULONG timecount;     // counts 10ms intervalls
  int bytes,transfer,count,retrycnt;
  UBYTE error;
  UWORD delay = 0; // No need to delay here.
  UWORD speed;
  
  // Retry several times before giving up
  retrycnt  = 15;
  do {
    // First set the error code: Device does not exist.
    error       = 0x8a;	
    // Initialize the timeout. If we wait longer than this, then we
    // retry the command. timeoutsecs is approximately in seconds
    // for the Atari, here precisely in seconds. We delay 10ms at
    // once, then run thru a display/sound VBI
    timeout.StartTimer(0,10*1000);   
    // Compute the number of 10ms intervals until we
    // get a timeout. One second has 100 of them.
    timecount   = timeoutsecs * 100;   
    // First compute the command frame from the data
    cmdframe[0] = UBYTE(device + unit - 1);
    cmdframe[1] = command;
    cmdframe[2] = UBYTE(aux);
    cmdframe[3] = UBYTE(aux >> 8);
    //
    // Find now the corresponding active device
    if ((ser = FindDevice(cmdframe))) {
      // Check whether the device accepts this command and
      // check for the number of bytes in a data frame.
      DataFrameLength = 0;
      speed           = Baud19200;
      cmdtype         = ser->CheckCommandFrame(cmdframe,DataFrameLength,Baud19200);
      ReallocDataFrame(DataFrameLength);
      switch(cmdtype) {
      case Off:
	// Device does not exist on the bus. No need to retry this,
	// if its not here, then it isn't.
	return 0x8A;
      case InvalidCommand:
	// The device didn't want to complete the command. Since even
	// the AtariSIO command returns immediately sending the command
	// in the background, this is also the last word we may have
	// spoken.
	return 0x8b; // was: 0x90
      case ReadCommand:
      case FormatCommand:
	// Test whether the device still likes the command.
	if (ser->AcknowledgeCommandFrame(cmdframe,delay,speed) != 'A')
	  return 0x8b;
	// Complete a read now.
	// Toggle to the ReadFrame, read data and return it. First
	// signal whether we could read the data as expected. For
	// this check whether we use single byte transfer or block
	// transfer.     
	bytes  = DataFrameLength;
	if (bytes == 0) {
	  bytes = 1; // single byte transfer shall read single bytes
	} else if (bytes != size) {
	  //return 0x8e; 
	  // Return a framing error if the size doesn't fit.
	  // This is also a hard error because the software interface layer
	  // of the SerialDevice and the Atari program do not fit.
	  // FIX: Silently accept, do not deliver an error. This must be done
	  // to pass the 1050 diag test.
	  return 0x01;
	}
	//
	buffer = DataFrame;
	count  = 0;
	do {
	  transfer = bytes;
	  result   = ser->ReadBuffer(cmdframe,buffer,transfer,delay,speed);
	  bytes   -= transfer;
	  buffer  += transfer;
	  count   += transfer;
	  //
	  if (transfer == 0) {
	    // Did not transfer any bytes. Test whether we timed
	    // out. If so, generate a timeout error.
	    if (WaitForSerialDevice(&timeout,timecount)) {
	      error = 0x8a; // timeout error;
	      break;
	    }
	  }
	  // We may not abort until the result becomes non-zero
	  // as the direct IO implementations require the extra
	  // calls to check for the checksum.
	} while(result == 0);
	// 
	// If the read command signaled success, insert
	// the bytes into the user RAM by means of "DMA".
	if (result == 'C' && count != size) {
	  error = 0x8e; // framing error.
	}
	// FIX: SIO also inserts the data in case of invalid
	// reads, so we do the same here.
	if (result == 'C' || error != 0x8a) {
	  // Worked fine. Now place the data into the RAM by
	  // means of "DMA".
	  ram    = machine->MMU()->CPURAM();
	  buffer = DataFrame;
	  // Set count = size, even in error cases for
	  // safety reasons.
	  if ((count = size)) { 
	    do {
	      ram->WriteByte(UWORD(mem++),*buffer++);
	    } while(--count);
	  }
	}
	// Now return the status indicator.
	// Either OK or Device NAK
	if (result)
	  error = (result == 'C')?0x01:0x90;
	break;
      case WriteCommand:
	// Test whether the device still likes the command.
	if (ser->AcknowledgeCommandFrame(cmdframe,delay,speed) != 'A')
	  return 0x8b;
	// Complete a write now. For that, we
	// need to distinguish between single byte
	// transfer and block transfer.
	if (DataFrameLength) {
	  // Block transfer. This is most convenient as we
	  // don't need to 'poll'. Instead, we can read
	  // a block by "DMA".
	  if (size != DataFrameLength) {
	    return 0x8e; // signal framing error if the size doesn't fit.
	  }
	  bytes  = DataFrameLength;
	  buffer = DataFrame;
	  ram    = machine->MMU()->CPURAM();
	  if (bytes) {
	    do {
	      *buffer = UBYTE(ram->ReadByte(UWORD(mem++)));
	      buffer++;
	    } while(--bytes);
	  }
	  //
	  // No timeout required for block commands. these complete
	  // in "one go".
	  //
	  // Now write the buffer back to the device until all
	  // data has been written or a problem has been
	  // reported.
	  result = ser->WriteBuffer(cmdframe,DataFrame,DataFrameLength,delay,Baud19200);
	  count  = DataFrameLength;
	} else {
	  UBYTE dat;
	  // Single byte transfer. Transfer data until the device
	  // stops us. I must here explicitly write the checksum
	  // into the device.
	  //
	  ram    = machine->MMU()->CPURAM();
	  count  = 0;
	  do {
	    transfer = 1; // single bytes at once.
	    dat      = UBYTE(ram->ReadByte(UWORD(mem)));
	    result   = ser->WriteBuffer(cmdframe,&dat,transfer,delay,Baud19200);
	    count   += transfer;
	    mem     += transfer;	  
	    if (transfer == 0) {	  
	      // Did not transfer any bytes. Test whether we timed
	      // out. If so, generate a timeout error.
	      if (WaitForSerialDevice(&timeout,timecount)) {
		error = 0x8a; // timeout error;
		break;
	      }
	    }
	    // We need to repeat until we get a status. The
	    // direct IO classes might require the extra
	    // iterations.
	  } while(result == 0);
	}
	// If we transmitted overall more bytes than requested,
	// then something strange is going on and we'd better
	// signal an error. DO NOT interrupt the traffic as this
	// might crash the device.
	if (result == 'C' && count != size) {
	  error = 0x8e;
	}
	// Now flush the contents. This may also take a while.
	if (result == 'A' || result == 'C') {
	  do {
	    result = ser->FlushBuffer(cmdframe,delay,speed);
	    if (timeout.EventIsOver()) {	      
	      // Test whether we timed out. 
	      // If so, generate a timeout error.
	      if (WaitForSerialDevice(&timeout,timecount)) {
		error = 0x8a; // timeout error;
		break;
	      }
	    }
	  } while(result == 0);
	}
	//
	// Check for the result code now.
	if (result == 'A' || result == 'C') {
	  error = 0x01; // is fine.
	} else if (result) {
	  // Return Device NAK if this doesn't fit.
	  // This is similar to what the Os does.
	  error = 0x90;
	}
	break;
      case StatusCommand:
	// This is similar to read except that zero bytes are transfered
	// and the result is immediate. AtariSIO doesn't support this
	// currently, so we don't care much
	switch (ser->ReadStatus(cmdframe,delay,speed)) {
	case 0x00:
	  return 0x8a; // device did not sync
	case 'E':
	  return 0x8b; // device returned an error
	case 'C':
	  return 1;    // device worked.
	}
      } // End of switch on command type. 
    } else {
      // Devide not present, or did not accept the command type,
      // return immediately.
      return 0x90;
    }
    //
    // Here test the error code. If the result is fine, return
    // immediately.
    if (error == 0x01)
      return error;
    //
    // Every fourth error, try whether resetting the device
    // helps.
    if ((error & 0x03) == 0)
      ser->WarmStart();
    // Otherwise, count the retry count down.
  } while(--retrycnt);
  //
  // Return the error code we collected.
  return error;
}
///

/// SIO::ConcurrentRead
// Test whether a serial byte from concurrent mode is available. Return
// true if so, and return the byte.
bool SIO::ConcurrentRead(UBYTE &data)
{
  class SerialDevice *dev;
  
  // Check all serial devices for which one is able to
  // deliver a serial byte.
  for(dev = Devices.First();dev;dev = dev->NextOf()) {
    if (dev->ConcurrentRead(data))
      return true;
  }
  return false;
}
///

/// SIO::ConcurrentWrite
// Output a serial byte thru concurrent mode over the channel.
void SIO::ConcurrentWrite(UBYTE data)
{
  class SerialDevice *dev;
  //
  // Ask all serial devices for delivering a concurrent byte.
  // If one is found, deliver it. Otherwise, warn that there
  // is a concurrent put without a dedicated receiver.
  for(dev = Devices.First();dev;dev = dev->NextOf()) {
    if (dev->ConcurrentWrite(data)) {
      HaveWarned = false;
      return;
    }
  }
  //
  if (!HaveWarned) {
    machine->PutWarning("Unrequested concurrent write of byte $%02x.\n",data);
    HaveWarned = true;
  }
}
///

/// SIO::TapeWrite
// Output a byte from the tape recorder.
void SIO::TapeWrite(UBYTE data)
{
  class SerialDevice *dev;
  //
  // Ask all serial devices whether there is one that takes
  // FM-coded (two-tone) data.
  if (MotorEnabled) {
    for(dev = Devices.First();dev;dev = dev->NextOf()) {
      if (dev->TapeWrite(data)) {
	HaveWarned = false;
	return;
      }
    }
  }
  //
  if (!HaveWarned) {
    machine->PutWarning("Unrequested two-tone data write of byte $%02x.\n",data);
    HaveWarned = true;
  }
}
///

/// SIO::SetMotorLine
// Enable or disable the state of the tape motor,
// thus potentially running or stopping the
// motor.
void SIO::SetMotorLine(bool onoff)
{
  // If onoff is false, the motor is running.
  MotorEnabled = !onoff;
}
///

/// SIO::DisplayStatus
// Print the status of the chip over the monitor
void SIO::DisplayStatus(class Monitor *mon)
{
  const char *framename;

  switch(SIOState) {
  case NoState:
    framename = "no frame pending";
    break;
  case CmdState:
    framename = "command frame";
    break;
  case AcknowledgeState:
    framename = "command acknowledge";
    break;
  case StatusState:
    framename = "data acknowledge";
    break;
  case ReadState:
    framename = "reading";
    break;
  case WriteState:
    framename = "writing";
    break;
  case FlushState:
    framename = "write acknowledge";
    break;
  case DoneState:
    framename = "write done";
    break;
  default:
    framename = "undefined";
    break;
  }

  mon->PrintStatus("SIO Status:\n"
		   "\tSIO Status : %s\n"
		   "\tSIO SerIn Command Delay: " ATARIPP_LD "\n"		   
		   "\tSIO Read Done Delay    : " ATARIPP_LD "\n"
		   "\tSIO Write Done Delay   : " ATARIPP_LD "\n"
		   "\tSIO Format Done Delay  : " ATARIPP_LD "\n"
		   "\tSIO Command Frame Idx  : %d\n"
		   "\tSIO Data Frame Idx     : %d\n"
		   "\tCommand Frame Contents : %02x %02x %02x %02x\n",
		   framename,
		   SerIn_Cmd_Delay,
		   Read_Done_Delay, 
		   Write_Done_Delay,
		   Format_Done_Delay,
		   CommandFrameIdx,
		   DataFrameIdx, 
		   CommandFrame[0],CommandFrame[1],
		   CommandFrame[2],CommandFrame[3]
		   );
}
///

/// SIO::ParseArgs
// Implement the functions of the configurable class
void SIO::ParseArgs(class ArgParser *args)
{
  args->DefineTitle("SIO");
  
  args->DefineLong("SerInCmdDelay","serial command accept delay in HBlanks",
		   0,240,SerIn_Cmd_Delay);
  args->DefineLong("ReadDoneDelay","serial read delay in HBlanks",
		   0,240,Read_Done_Delay);
  args->DefineLong("ReadDeliverDelay","serial deliver data delay in HBlanks",
		   0,10,Read_Deliver_Delay);
  args->DefineLong("WriteDoneDelay","serial write delay in HBlanks",
		   0,240,Write_Done_Delay);
  args->DefineLong("FormatDoneDelay","format done delay in HBlanks",
		   0,1024,Format_Done_Delay);
}
///
