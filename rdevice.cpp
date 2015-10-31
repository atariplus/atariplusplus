/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: rdevice.cpp,v 1.6 2014/03/16 14:54:08 thor Exp $
 **
 ** In this module: R: emulated device
 **********************************************************************************/

/// Includes
#include "rdevice.hpp"
#include "interfacebox.hpp"
#include "sio.hpp"
#include "string.hpp"
#include "cpu.hpp"
#include "mmu.hpp"
#include "adrspace.hpp"
///

/// RDevice::RDevice
// Initialize the R: handler interface
RDevice::RDevice(class Machine *mach,class PatchProvider *p)
  : Device(mach,p,'R','R'), HBIAction(mach),
    serial(NULL), buffer(NULL), buffersize(0), inputbuffer(new UBYTE[4096])
{
  Reset();
}
///

/// RDevice::Reset
// Reset the connection of the R: handler
void RDevice::Reset(void)
{
  isopen        = false;
  concurrent    = false;
  parityerror   = false;
  overrun       = false;
  dmabuflen     = 0;
  bufferedbytes = 0;
  insertpos     = 0;
  removepos     = 0;
  serial        = NULL;
}
///

/// RDevice::~RDevice
RDevice::~RDevice(void)
{
  delete[] buffer;
  delete[] inputbuffer;
}
///

/// RDevice::ComputeParity
// Compute the parity for the input data with the indicated number of bits.
// Returns whether the number of one-bits is even (then false) or odd (then true).
bool RDevice::ComputeParity(UBYTE value)
{
  int cnt  = 0;
  int size = databits;

  do {
    if (value & 1)
      cnt++;
    value >>= 1;
  } while(--size);

  return (cnt & 1)?true:false;
}
///

/// RDevice::EnlargeBuffer
// Enlarge the input/output buffer to at least the indicated amount of bytes
void RDevice::EnlargeBuffer(int datasize)
{  
  if (datasize > buffersize) {
    UBYTE *newbuffer = new UBYTE[datasize];
    if (buffer) {
      memcpy(newbuffer,buffer,buffersize);
      delete[] buffer;
    }
    buffer = newbuffer;
    buffersize = datasize;
  }
}
///

/// RDevice::RunCommand
// Forward a SIO command to the interface box, returns the CIO
// error code.
UBYTE RDevice::RunCommand(UBYTE cmd,UBYTE aux1,UBYTE aux2,int &size)
{
  UWORD delay = 0;
  
  if (serial) {
    UBYTE cmdframe[4]; // Build a command frame.
    SIO::CommandType cmdtype;
    int datasize = 0;
    UBYTE status = 'C';
    UWORD speed  = SIO::Baud19200;
    //
    cmdframe[0]  = 0x50; // The SIO identifier of the 850 interface box.
    cmdframe[1]  = cmd;
    cmdframe[2]  = aux1;
    cmdframe[3]  = aux2;
    cmdtype      = serial->CheckCommandFrame(cmdframe,datasize,SIO::Baud19200);
    //
    switch(cmdtype) {
    case SIO::Off:
      // Reply with timeout in case the box is not available.
      return 0x8a;
    case SIO::InvalidCommand:
      // Reply with Device NAK.
      return 0x90;
    case SIO::ReadCommand:
    case SIO::FormatCommand:
      // Do we need a new buffer? If so, enlarge.
      EnlargeBuffer(datasize);
      // Read the bytes into the system.
      delay    = 0;
      status   = serial->ReadBuffer(cmdframe,buffer,datasize,delay,speed);
      size     = datasize;
      break;
    case SIO::WriteCommand:
      // Transfer data from our buffer to the device.
      delay    = 0;
      status   = serial->WriteBuffer(cmdframe,buffer,size,delay,SIO::Baud19200);
      break;
    case SIO::StatusCommand:
      // Just a simple status command, no data phase here.
      delay    = 0;
      status   = serial->ReadStatus(cmdframe,delay,speed);
    }
    //
    // Check for the status, possibly create an error code.
    switch(status) {
    case 'A':
    case 'C':
      return 0x01; // Worked.
    case 0x00:
      // Out of sync.
      return 0x8b;
    default:
      // Device NAK.
      return 0x90;
    }
  } else {
    // Channel not open.
    return 0x85;
  }
}
///

/// RDevice::HBI
// The following HBI is used to pull bytes from the serial interface and
// place them into the buffer.
void RDevice::HBI(void)
{
  if (isopen && concurrent) {
    UBYTE data;
    UWORD buflen;
    //
    // Find the buffer len. Either our internal, or that of the provided DMA buffer.
    buflen = (dmabuflen)?(dmabuflen):4096;
    //
    // Check whether there is any data in the serial input buffer.
    while(serial->ConcurrentRead(data)) {
      UWORD next = insertpos + 1;
      if (next  >= buflen)
	next     = 0;
      //
      // If the next buffer position hits again the reading position,
      // we have a buffer overrun pending.
      if (insertpos == next) {
	overrun = true;
      } else {
	// Insert the data into the buffer.
	if (dmabuflen) {
	  cpumem->WriteByte(dmabuffer + next,data);
	} else {
	  // Otherwise, the handler-internal buffer
	  inputbuffer[next] = data;
	}
	bufferedbytes++;
	insertpos = next;
      }
    }
  }
}
///

/// RDevice::Open
// Open the interface box for communication
UBYTE RDevice::Open(UBYTE,UBYTE unit,char *,UBYTE aux1,UBYTE)
{
  UBYTE status;
  int size;
  serial = Machine->InterfaceBox();
  //
  // There is only one unit we emulate here.
  if (unit != 1)
    return 0xa0;
  //
  // Check whether we are open already. We currently only support one serial
  // channel.
  if (isopen) {
    return 0x96; // channel is open
  }
  if ((aux1 & 0x0c) == 0) {
    // Must at least or write something.
    return 0xb1; // invalid mode
  }
  openmode      = aux1;
  databits      = 8;
  transposition = 0; // default operation
  dmabuflen     = 0;
  bufferedbytes = 0; 
  insertpos     = 0;
  removepos     = 0;
  concurrent    = false;
  parityerror   = false;
  overrun       = false;
  invreplace    = ' ';
  //
  // Dummy write of zero bytes to stop the concurrent mode.
  size          = 0; // Nothing in the buffer.
  status        = RunCommand('W',0,0,size);
  if (status   != 1)
    return status;
  // Reset to 300 baud, eight bits, one stop bit, no handshaking.
  status        = RunCommand('B',0,0,size);
  if (status   != 1)
    return status;
  // Reset DTR, RTS, XMT to all one.
  status        = RunCommand('A',0xff,0,size);
  if (status   != 1)
    return status;
  //
  // Is now open
  isopen        = true;
  return 0x01;
}
///

/// RDevice::Close
// Close the interface box channel.
UBYTE RDevice::Close(UBYTE)
{
  UBYTE status = 0x01;
  //
  concurrent = false;
  //
  // Only if it is open. Ignore otherwise.
  if (isopen) {
    int contents = 0;
    // Run a dummy write to reset the concurrent mode.
    status = RunCommand('W',0,0,contents);
  }
  //
  isopen     = false;
  return status;
}
///

/// RDevice::Get
// Read a character buffered from the interface box
UBYTE RDevice::Get(UBYTE,UBYTE &value)
{
  UBYTE data;
  //
  // Works only if the channel is open and the concurrent mode is available.
  if (!concurrent)
    return 0x9a;
  //
  // Check whether there is data waiting at the serial port.
  if (bufferedbytes == 0) {
    class CPU *cpu      = Machine->CPU();
    class AdrSpace *adr = Machine->MMU()->CPURAM();
    // Check whether break has been hit.
    if (adr->ReadByte(0x11) == 0) {
      return 0x80; // break-abort.
    } else {
      ADR   pc   = cpu->PC() - 1 - 2; // the -1 is due to the implicit RTS, -2 is the size of the instruction
      UBYTE stck = cpu->S();
      //
      // Is not. Ok, we cannot continue with the program execution until
      // something is happening here. Thus, let the CPU re-run this instruction.
      adr->WriteByte(0x100 + stck--,UBYTE((pc) >> 8));
      adr->WriteByte(0x100 + stck--,UBYTE((pc) & 0xff));
      cpu->S() = stck;
      value    = 0x00;
      return 0x01;
    }
  } else {
    UWORD next,buflen;
    bool paritybit;
    //
    // Get the buffer len. Either the internal, or the "DMA" buffer supplied
    // by the user.
    buflen    = (dmabuflen)?(dmabuflen):4096;
    //
    // Extract the next byte from the buffer.
    next      = removepos + 1;
    if (next >= buflen)
      next    = 0;
    //
    // Now retrieve the data from the buffer.
    if (dmabuflen) {
      data = cpumem->ReadByte(dmabuffer + next);
    } else {
      // Otherwise, the handler-internal buffer
      data = inputbuffer[next];
    }
    bufferedbytes--;
    removepos = next;
    //
    // Compute the status of the parity bit in the intput data.
    paritybit = (data & (1 << databits))?(true):(false);
    //
    // Check for active character transposition now.
    // First handle the parity check.
    switch (transposition & 0x0c) {
    case 0x00: // Keep parity bit alive.
      break;
    case 0x0c: // Clear parity bit.
      value &= ~(1 << databits);
      break;
    case 0x08: // even parity.
      if (ComputeParity(value) != paritybit)
	parityerror = true;
      value &= ~(1 << databits);
      break;
    case 0x04: // odd parity.
      if (ComputeParity(value) == paritybit)
	parityerror = true;
      value &= ~(1 << databits);
      break;
    }
    //
    // Now perform the ASCII->ATASCII translation, if any.
    switch (transposition & 0x30) {
    case 0x20:
    case 0x30:
      // No translation.
      break;
    case 0x10:
      // Full translation. 
      if (data == 0x0d) {
	data = 0x9b; // Turn into EOL.
      } else {
	data &= 0x7f;
	if (data < 0x20 || data >= 0x7d) {
	  data = invreplace; // replacement character.
	}
      }
      break;
    case 0x00:
      // Partial translation only.
      if (data == 0x0d) {
	data = 0x9b; // Turn into EOL.
      }
      break;
    }
    value = data;
    return 0x01;
  }
}
///

/// RDevice::PutByte
// Write a single byte out to the serial output buffer,
// return the status.
UBYTE RDevice::PutByte(UBYTE value)
{
  //
  // Check for parity insertion.
  if (transposition & 0x03) {
    // At least some parity operations to be performed here.
    // First, turn upper bits into stop bits which are not part
    // of the data.
    value |= 0xff << databits;
    switch(transposition & 0x03) {
    case 0x01:
      // Odd parity. If already an odd number of bits is set, clear the parity bit
      // to keep it odd.
      if (ComputeParity(value))
	value &= ~(1 << databits);
      break;
    case 0x02:
      // Even parity. Reverse of the above.
      if (!ComputeParity(value))
	value &= ~(1 << databits);
      break;
    }
  }
  //
  // Is the concurrent mode enabled?
  if (concurrent) {
    if (serial->ConcurrentWrite(value))
      return 0x01;
    return 0x8c;
  } else {
    int size = 1;
    // Use the short-block mode to write this data. In principle,
    // we could buffer larger amounts of data, but since there is no
    // SIO serial overhead here, blocks of size one work as well.
    EnlargeBuffer(size);
    *buffer = value;
    return RunCommand('W',0,0,size);
  }
}
///

/// RDevice::Put
// Write a character out to the interface box
UBYTE RDevice::Put(UBYTE,UBYTE value)
{
  UBYTE status = 0x01;
  //
  if (!isopen)
    return 0x85; // Not open
  //
  // Check whether we are open for output.
  if ((openmode & 0x08) == 0)
    return 0x87; // open for input only.
  //
  // Now check for character transposition.
  if (value == 0x9b) {
    // Check whether we have at least EOL translation.
    if ((transposition & 0x30) <= 0x10) {
      // Insert CR before an LF?
      if (transposition & 0x40) {
	if (status == 1)
	  status = PutByte(0x0d);
	if (status == 1)
	  status = PutByte(0x0a);
      }
      if (status == 1)
	status = PutByte(0x0d);
      return status;
    } else {
      // No translation.
      return PutByte(0x9b);
    }
  }
  // Full translation?
  if ((transposition & 0x30) == 0x10) {
    // Full translation. Ingore non-ASCII characters.
    if (value < 0x20 || value >= 0x7d)
      return 0x01;
  } else if ((transposition & 0x30) == 0x00) {
    // Partial translation. Mask off the INVERSEVID bit.
    value &= 0x7f;
  }
  //
  return PutByte(value);
}
///

/// RDevice::Status
// Return the status of the RDevice now
UBYTE RDevice::Status(UBYTE)
{	
  class AdrSpace *adr = Machine->MMU()->CPURAM();
  UBYTE errorflag     = 0;
  UBYTE status        = 0x01;

  // Construct the error flag
  if (parityerror)
    errorflag        |= 0x20;
  if (overrun)
    errorflag        |= 0x10;
  
  if (concurrent) {
    // Concurrent mode is active
    adr->WriteByte(0x2ea,errorflag);
    adr->WriteByte(0x2eb,bufferedbytes & 0xff);
    adr->WriteByte(0x2ec,bufferedbytes >> 8);
    // No bytes in the output buffer.
    adr->WriteByte(0x2ed,0);
  } else {
    UBYTE status;
    int size = 0;
    //
    status = RunCommand('S',0,0,size);
    if (status == 1) {
      if (size != 2) 
	return 0x8e;
      adr->WriteByte(0x2ea,buffer[0] | errorflag);
      adr->WriteByte(0x2eb,buffer[1]);
    }
  }

  parityerror = false;
  overrun     = false;
  return status;
}
///

/// RDevice::Special
// Other input/output control mechanisms.
UBYTE RDevice::Special(UBYTE,UBYTE unit,class AdrSpace *adr,UBYTE cmd,ADR mem,UWORD len,UBYTE aux[6])
{
  UBYTE result = 0x01;
  //
  // There is only one unit we emulate here.
  if (unit != 1)
    return 0xa0;
  //
  // Re-read the serial interface method.
  serial = Machine->InterfaceBox();
  //
  switch(cmd) {
  case 32:
    // Write the output buffer out if pending.
    if (isopen) {
      if (openmode & 0x08) {
	if (serial->Drain()) {
	  result = 0x01;
	} else {
	  result = 0x8e;
	}
      } else {
	result = 0x87; // open for input only
      }
    } else {
      result = 0x85; // channel not open
    }
    break;
  case 34:
    // Set the status of the serial status lines.
    {
      int size = 0;
      result   = RunCommand('A',aux[0],aux[1],size);
    }
    break;
  case 36:
    // Define the baud rate for transmission.
    {
      int size = 0;
      databits = 8 - ((aux[0] >> 4) & 0x03);
      result   = RunCommand('B',aux[0],aux[1],size);
    }
    break;
  case 38:
    // Define parity and character transpositions.
    transposition = aux[0];
    invreplace    = aux[1];
    result        = 0x01;
    break;
  case 40:
    // Enter the concurrent mode.
    if (isopen) {
      if (concurrent) {
	result = 0x99; // concurrent mode already active.
      } else {
	if ((openmode & 0x01) == 0) {
	  result = 0x97; // R: hasn't been opened for concurrent mode.
	} else {
	  int size;
	  //
	  if ((openmode & 0x04)) {
	    // We do expect to read data.
	    if (aux[0] && len) {
	      ADR bufstart = mem;
	      ADR bufend   = mem + len;
	      if (UWORD(bufend) < UWORD(bufstart)) {
		// Overflow error, invalid buffer.
		result = 0x98;
	      } else {
		dmabuffer = mem;
		dmabuflen = len;
		cpumem    = adr;
	      }
	    } else {
	      dmabuffer = 0x00;
	      dmabuflen = 0;
	    }
	  } 
	  insertpos     = 0;
	  removepos     = 0;
	  //
	  // We are now in concurrent mode.
	  concurrent    = true;
	  // Now enable the concurrent mode.
	  result = RunCommand('X',openmode,0,size);
	}
      }
    } else {
      result = 0x85; // channel not open
    }
    break;
  default:
    result = 0x84; // Invalid command
  }
  //
  // In case the channel was open before, fix ZAUX1 so CIO remains
  // happy when reading or writing.
  if (isopen) {
    adr->WriteByte(0x2a,openmode);
  }
  return result;
}
///
