/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atarisioport.cpp,v 1.19 2015/08/15 14:52:35 thor Exp $
 **
 ** In this module: This is the controlling unit for Matthias Reichl's
 ** atarisio interface. This class keeps the file handle for AtariSIO.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "atarisioport.hpp"
#include "monitor.hpp"
#include "exceptions.hpp"
#include "chip.hpp"
#include "timer.hpp"
#include "serialstream.hpp"
#include "unistd.hpp"
#ifdef HAVE_ATARISIO_H
#include <stdint.h>
#include <atarisio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif
///

/// AtariSIOPort::AtariSIOPort
AtariSIOPort::AtariSIOPort(class Machine *mach)
  : Chip(mach,"SIOCable"), SioFD(-1), SerialStream(NULL),
    EnableSIO(true), DirectSerial(false), ProSystem(false), 
    DeviceName(new char[11]), 
    CmdToDataDelay(900), CmdFrameDelay(850 + 2600 + 1700), // See below for these figures.
    WaitCmdFrame(false)
{
  strcpy(DeviceName,SerialStream::SuggestName());  
}
///

/// AtariSIOPort::~AtariSIOPort
AtariSIOPort::~AtariSIOPort(void)
{
  // Check whether we must close down the channel here
#ifdef HAVE_ATARISIO_H
  if (SioFD >= 0) {
    close(SioFD);
  }
#endif
  delete SerialStream;
  delete[] DeviceName;
}
///

/// AtariSIOPort::WarmStart
// Reset the port by shutting it down. The next
// sio command will open it again.
void AtariSIOPort::WarmStart(void)
{
  if (SerialStream) {
    // Reset the command frame for good measure
    SetCommandLine(false);
    // Flush the contents of the buffer before we
    // close
    SerialStream->Flush();
    delete SerialStream;
    SerialStream = NULL;
  }
}
///

/// AtariSIOPort::ColdStart
// Another reset, this time for coldstart.
void AtariSIOPort::ColdStart(void)
{
  // runs into the warmstart
  WarmStart();
}
///

/// AtariSIOPort::OpenChannel
// Re-open the SIO buffer and initialize it
void AtariSIOPort::OpenChannel(void)
{
#ifdef HAVE_ATARISIO_H
  if (SioFD >= 0) {
    close(SioFD);
    SioFD = -1;
  }
#endif
  delete SerialStream;
  SerialStream = NULL;
  //
  if (EnableSIO) {
    // Check whether we have direct serial access.
    if (DirectSerial) {
      //
      // Open a serial channel
      SerialStream = new class SerialStream;
      if (SerialStream->Open(DeviceName)) {
	// Default is no HW handshake, 8N1
	if (!SerialStream->SetBaudRate(19200)) {
	  machine->PutWarning("Unable to switch device %s to 19200 baud,\n"
			      "disabling the AtariSIO emulator interface for now.",DeviceName);
	  delete SerialStream;
	  SerialStream = NULL;
	  EnableSIO = false;
	}
	// Turn off the command line
	SetCommandLine(false);
	//
	// Wait a little bit longer to
	// ensure that the server/drive gets reset.
      } else {
	machine->PutWarning("Unable to open the serial port %s,\n"
			    "disabling the AtariSIO emulator interface for now.",DeviceName);
	EnableSIO = false;
      }
    } else {
#ifdef HAVE_ATARISIO_H
      int error;
      // Ok, try to open me. Should we use anything fancy 
      // like O_NDELAY here?
      SioFD = open("/dev/atarisio",O_RDWR);
      if (SioFD >= 0) {
	unsigned int mode;
	int version;
	// That worked. Install the various modes now.
	// First go for the cable mode.
	mode    = (ProSystem)?(ATARISIO_MODE_PROSYSTEM):(ATARISIO_MODE_1050_2_PC);
	error   = ioctl(SioFD,ATARISIO_IOC_SET_MODE,mode);
	if (error < 0) {
	  // This failed. Report an error.
	  ThrowIo("AtariSIOPort::OpenChannel",ErrorString(errno));
	}
	version = ioctl(SioFD,ATARISIO_IOC_GET_VERSION);
	if (version < 0) {
	  ThrowIo("AtariSIOPort::OpenChannel",ErrorString(errno));
	}
	// Check whether the version match. We only need to test the bits 15..8 containing
	// the major version
	if ((version^ATARISIO_VERSION) & 0xff00) {
	  Throw(InvalidParameter,"AtariSIOPort::OpenChannel","not compiled with the installed AtariSIO version");
	}
	//
	// AutoBaud mode only for the server mode, no need to do
	// this here.
      } else {
	machine->PutWarning("The kernel interface for /dev/atarsio does not open or is not available.\n"
			    "Disabling the AtariSIO emulator interface for now.");
	EnableSIO = false;
      }
#endif
    }
  }
}
///

/// AtariSIOPort::Flush
// Flush the contents of the serial output buffer; this
// disposes the buffer contents.
void AtariSIOPort::Flush(void)
{
  if (EnableSIO && SerialStream) {
    SerialStream->Flush();
  }
}
///

/// AtariSIOPort::Drain
// Drain the output buffer. This waits until all the buffer
// contents has been written out.
void AtariSIOPort::Drain(void)
{
  if (EnableSIO && SerialStream) {
    if (!SerialStream->Drain()) {
      ThrowIo("AtarSIOPort::Drain","unable drain the output buffer");
    }
  }
}
///

/// AtariSIOPort::SetCommandLine
// Set the status of the command line, on or off
// This is for direct serial access.
void AtariSIOPort::SetCommandLine(bool on)
{
  bool error = false;
  //
  if (EnableSIO && SerialStream) {
    if (ProSystem) {
      // Always enable RTS, DRT is the state of the command line.
      if (!SerialStream->SetRTSState(true))
	error = true;
      if (!SerialStream->SetDTRState(on))
	error = true;
    } else {
      // Never enable DTR, RTS is the state of the command line.
      if (!SerialStream->SetDTRState(false))
	error = true;
      if (!SerialStream->SetRTSState(on))
	error = true;
    }
  }
  if (error) {
    ThrowIo("AtariSIOPort::SetCommandLine","unable to set the state of the COMMAND line");
  }
}
///

/// AtariSIOPort::TransmitCommandFrame
// Transmit a command frame in direct IO.
void AtariSIOPort::TransmitCommandFrame(const UBYTE *cmdframe)
{
  if (EnableSIO) {
    int i,sum;
    if (SerialStream == NULL || !SerialStream->isOpen()) {
      OpenChannel();
    }
    if (SerialStream && SerialStream->isOpen()) {
      UBYTE frame[5];
      // The command frame is four bytes LONG, plus a fifth for the 
      // checksum we need to compute here.
      // Dispose all the I/O
      Flush();
      // Set the CMD line
      SetCommandLine(true);
      // Delay for approximately 900us
      SerialTime.StartTimer(0,CmdToDataDelay);
//
      // The command frame is five bytes LONG, plus a fifth for the 
      // checksum we need to compute here.
      for(i = 0,sum = 0;i < 4;i++) {
	sum += (frame[i] = cmdframe[i]);
	if (sum >= 256) {
	  sum -= 255; // include the carry.
	}
      }
      // Include the checksum.
      frame[i] = sum; 
      // Ok, be a bit nasty. The kernel doesn't allow us to wait for
      // less than 10ms savely, thus busy wait. Urgl!  
      while(SerialTime.EventIsOver() == false){};
      // Start now the serial timer for the command line
      // transfer. We then later wait until the command is
      // over. This includes the time for the bytes to be
      // send. 850us for the serial timer,
      // five bytes at 9600 in 8N1 = 50 bits makes 2604.167
      // usecs for the transmission of the frame.
      if (SerialStream) {
	SerialTime.StartTimer(0,CmdFrameDelay);
	// Wait for the command frame to be completed on the next
	// read/write.
	WaitCmdFrame = true;
	//fprintf(stderr,"f");
	//
	if (SerialStream->Write((unsigned char *)&frame,sizeof(frame)) != sizeof(frame)) {
	  machine->PutWarning("Unable to transmit a serial command frame,\n"
			      "disabling AtariSIO for now.\n");
	  // At least, lower the command line to avoid
	  // confusion at Atari devices.
	  SetCommandLine(false);
	  delete SerialStream;
	  SerialStream = NULL;
	  EnableSIO    = false;
	  WaitCmdFrame = false;
	}
      }
    }
  }
}
///

/// AtariSIOPort::ErrorString
// Convert an AtariOs error to a more informative string
const char *AtariSIOPort::ErrorString(int error)
{
  if (error < 1024) {
    // Ok, this error has been generated by the kernel,
    // use the strerror interface as usual.
    return strerror(error);
  } else {
#ifdef HAVE_ATARISIO_H
    switch(error) {
    case EATARISIO_ERROR_BLOCK_TOO_LONG:
      return "io transfer block too LONG";
    case EATARISIO_COMMAND_NAK:
      return "device negative acknowledge";
    case EATARISIO_COMMAND_TIMEOUT:
      return "command timeout";
    case EATARISIO_CHECKSUM_ERROR:
      return "checksum error";
    case EATARISIO_COMMAND_COMPLETE_ERROR:
      return "unknown command completion code";
    case EATARISIO_DATA_NAK:
      return "data negative acknowledge";
    default:
      return "unknown AtariSIO error";
    }
#else
    return "unknown AtariSIO error";
#endif
  }
}
///

/// AtariSIOPort::External
// Transmit a command to an external device by means
// of the kernel interface. Returns the result character
// of the external device.
// The first boolean describes the data direction. Unfortunately,
// AtariSIO knows only IN or OUT, not STATUS.
// The second array is the command frame,
// the next the source or the target buffer, the last the byte
// size. Timeout is timeout in seconds.
char AtariSIOPort::External(
#ifdef HAVE_ATARISIO_H
			    bool writetodevice,const UBYTE *commandframe,UBYTE *buffer,int size,UBYTE timeout
#else
			    bool,const UBYTE *,UBYTE *,int,UBYTE
#endif
			    )
{
#ifdef HAVE_ATARISIO_H
  SIO_parameters sioparams;
  int error;
  
  // Check whether the SIO channel is open. If it is not, try
  // to reopen it now.
  if (EnableSIO && SioFD < 0) {
    OpenChannel();
  }
  if (SioFD >= 0) {
    UBYTE command         = commandframe[1];
    // Fill in the parameters for the kernel interface.
    sioparams.device_id   = commandframe[0];
    sioparams.command     = command;
    sioparams.direction   = (writetodevice)?(1):(0);
    sioparams.timeout     = timeout;
    sioparams.aux1        = commandframe[2];
    sioparams.aux2        = commandframe[3];
    sioparams.data_length = size;
    sioparams.data_buffer = buffer;
    //
    // Now perform the ioctl.
    error = ioctl(SioFD,ATARISIO_IOC_DO_SIO,&sioparams);
    if (error < 0) {
      // Huh, an error. Try to translate this back into
      // the original drive error.
      switch(errno) {
      case EATARISIO_COMMAND_NAK:
      case EATARISIO_DATA_NAK:
	return 'N';
      default:
	return 'E';
      }
    }
    // Command complete.
    return 'C';
  } else {
    return 0;
  }
#else
  return 0;
#endif
}
///

/// AtariSIOPort::ParseArgs
// Parse off command line arguments global for all
// of the AtariSIO handling.
void AtariSIOPort::ParseArgs(class ArgParser *args)
{
  LONG cable = (ProSystem)?(1):(0);
  static const struct ArgParser::SelectionVector cablevector[] =
    {
      {"1050-2-PC",  0},
      {"ProSystem",  1},
      {NULL       ,  0}
    };

  args->DefineTitle("SIOCable");  
  args->DefineBool("EnableAtariSIO","enable or disable the AtariSIO interface",EnableSIO);
#ifdef HAVE_ATARISIO_H
  args->DefineBool("DirectSerial","enable user space serial access to SIO",DirectSerial);
#else
  DirectSerial = true;
#endif
  args->DefineString("DirectSerialDevice","serial device name for DirectSerial output",DeviceName);
  args->DefineLong("CmdToDataDelay","usecs from cmd frame start to cmd frame transfer",
		   0,2000,CmdToDataDelay);
  args->DefineLong("CmdFrameLength","size of a command frame in usecs",800,10000,CmdFrameDelay);
  args->DefineSelection("CableType","set the cable type that connects to the external device",
			cablevector,cable);
  // Convert the data into the internal representation.
  ProSystem     = (cable)?(true):(false);

  // Enforce reopening the channel by closing it
  WarmStart();
}
///

/// AtariSIOPort::DisplayStatus
// Display the status of this chip over the monitor
void AtariSIOPort::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("AtariSIOCable status :\n"
		   "\tKernel interface   : %s\n"
		   "\tExternal interface : %s\n"
		   "\tCable mode         : %s\n\n",
		   (SioFD >= 0)?("connected"):("disconnected"),
		   (EnableSIO)?("enabled"):("disabled"),
		   (ProSystem)?("ProSystem"):("1050-2-PC")
		   );
}
///

