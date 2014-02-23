/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: serialstream.cpp,v 1.13 2013/11/29 12:20:38 thor Exp $
 **
 ** In this module: An interface abstraction for serial ports,
 ** one level above the operating system. This here grands access
 ** to all major functions of a serial device, including input/output
 ** and control of the serial lines.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "serialstream.hpp"
#if defined(__unix__)
#include "unistd.hpp"
#include "termios.hpp"
#if HAVE_TERMIOS_H && HAVE_TCSETATTR && HAVE_TCGETATTR && HAVE_CFSETOSPEED && HAVE_CFSETISPEED && HAVE_FCNTL_H
#define UNIX 1
# include <fcntl.h>
# include "unistd.hpp"
# include <errno.h>
extern "C" {
# include <termios.h>
}
#ifndef HAS_IUCLC_DEFINE
#define IUCLC 0
#endif
#ifndef HAS_OLCUC_DEFINE
#define OLCUC 0
#endif
#ifndef HAS_OFILL_DEFINE
#define OFILL 0
#endif
#ifndef HAS_OFDEL_DEFINE
#define OFDEL 0
#endif
#ifndef HAS_XCASE_DEFINE
#define XCASE 0
#endif
#endif
#elif defined(WIN32)
#include <windows.h>
// Error counters. These should count errors since boot time, but
// these are at least global errors since program start.
static int framingerrors  = 0;
static int portoverruns   = 0;
static int parityerrors   = 0;
static int bufferoverruns = 0;
#endif
///

/// SerialHandle
// Os dependent handle of a serial
// stream, one level above the Os abstraction.
#if defined(__unix__)
struct SerialStream::SerialHandle {
  // Just the raw file descriptor
  int fd;
};
#elif defined(WIN32)
struct SerialStream::SerialHandle {
  HANDLE     fd;
  HANDLE     readevent;
  OVERLAPPED osreader;
  HANDLE     writeevent;
  OVERLAPPED oswriter;
  //
  bool       readpending;
  bool       writepending;
  //
  //
  unsigned char *nextavail; // next available byte in the input buffer
  unsigned char *lastavail; // first non-available byte, i.e. the byte behind the last one
  unsigned char inputbuffer [2048];
  unsigned char outputbuffer[2048];
};
#else
// Just an empty dummy otherwise
struct SerialStream::SerialHandle {
  // Just a dummy here.
  int fd;
};
#endif
///

/// SerialStream::SerialStream
// Create a new serial stream
SerialStream::SerialStream(void)
  : Stream(0)
{
}
///

/// SerialStream::~SerialStream
SerialStream::~SerialStream(void)
{
  if (Stream)
    Close();
}
///

/// SerialStream::SuggestName
// Suggest a name for the serial connection, to
// be used for the open path.
const char *SerialStream::SuggestName(void)
{
#if defined(__unix__)
  return "/dev/ttyS0";
#elif defined(WIN32)
  return "COM1:";
#else
  return "";
#endif
}
///

/// SerialStream::Open
// Open a new serial stream for read&write
// returns false in case of an error.
bool SerialStream::Open(const char *name)
{
  if (!Stream) {
    Stream = new struct SerialHandle;
#if defined(UNIX)
    Stream->fd = open(name,O_NOCTTY|O_RDWR);
    // We also need to define the serial transfer parameters here.
    // Non-cooked, no hardware flow-control, no software flow control.
    //
    if (Stream->fd >= 0) {
      struct termios t;
      speed_t s = B300;
      //
#if HAVE_ISATTY
      if (!isatty(Stream->fd)) {
	Close();
	return false;
      }
#endif
      //
      if (tcgetattr(Stream->fd,&t) == 0) {
	// We got the current parameters. Now install our own settings.
#if HAVE_CFMAKERAW 
	// The following is not available on Solaris, but strictly speaking
	// I don't need it anyhow. This is just a safety feature.
	cfmakeraw(&t); // I don't see how this could cause an error...
#endif
	t.c_iflag    &= ~(IGNBRK|IGNPAR|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IUCLC|IXON|IMAXBEL|INPCK);
	t.c_oflag    &= ~(OPOST|OLCUC|ONLCR|OCRNL|ONOCR|ONLRET|OFILL|OFDEL);
#ifdef HAS_CRTSCTS_DEFINE
	t.c_cflag    &= ~(CSIZE|CSTOPB|PARENB|HUPCL|CRTSCTS|CLOCAL);
#else
	t.c_cflag    &= ~(CSIZE|CSTOPB|PARENB|HUPCL|CLOCAL);
#endif
	t.c_cflag    |=   CREAD|CLOCAL; // do not monitor CarrierDetect
	// Set to eight data bits
	t.c_cflag    |= CS8;
	// One stop bit, no handshake
	t.c_lflag    &= ~(ISIG|ICANON|XCASE|ECHO|FLUSHO);
	// Return immediately, only the number of currently available
	// bytes
	t.c_cc[VMIN]  = 0;
	t.c_cc[VTIME] = 0;
	// Install parameters
	if (cfsetospeed(&t,s) == 0 && cfsetispeed(&t,s) == 0 && tcsetattr(Stream->fd,TCSANOW,&t) == 0)
	  return true;
      }
    }
    // Stream could not be opened. Shut down again.
    Close();
    return false;
#elif defined(WIN32)
    // NULL-Out the handles so we don't do a desaster if a part of this
    // operation fails.
    Stream->fd           = NULL;
    Stream->readevent    = NULL;
    Stream->writeevent   = NULL;
    Stream->readpending  = false;
    Stream->writepending = false;
    Stream->nextavail    = Stream->inputbuffer;
    Stream->lastavail    = Stream->inputbuffer;
    memset(&Stream->osreader,0,sizeof(Stream->osreader));
    memset(&Stream->oswriter,0,sizeof(Stream->oswriter));
    Stream->fd = CreateFile(name,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,
			    FILE_FLAG_OVERLAPPED, // means asynchronous access
			    0);
    if (Stream->fd != NULL) {
      Stream->readevent       = CreateEvent(NULL, TRUE, FALSE, NULL);
      if (Stream->readevent  != NULL) {
	Stream->writeevent    = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (Stream->writeevent != NULL) {
	  DCB dcb       = {0};
	  dcb.DCBlength = sizeof(dcb);
	  Stream->osreader.hEvent = Stream->readevent;
	  Stream->oswriter.hEvent = Stream->writeevent;
	  // Now adjust the serial communications settings to 300,8N1
	  if (GetCommState(Stream->fd, &dcb)) {
	    BuildCommDCB("baud=300 parity=N data=8 stop=1",&dcb);
	    dcb.BaudRate          = CBR_300;
	    dcb.ByteSize          = 8;
	    dcb.fAbortOnError     = false;
	    dcb.fBinary           = true;
	    dcb.fDsrSensitivity   = false;
	    dcb.fDtrControl       = DTR_CONTROL_DISABLE;
	    dcb.fInX              = false;
	    dcb.fOutX             = false;
	    dcb.fNull             = false;
	    dcb.fOutxCtsFlow      = false;
	    dcb.fOutxDsrFlow      = false;
	    dcb.fParity           = false;
	    dcb.fRtsControl       = RTS_CONTROL_DISABLE;
	    dcb.Parity		  = NOPARITY;
	    dcb.StopBits          = ONESTOPBIT;
	    if (SetCommState(Stream->fd, &dcb)) {
	      COMMTIMEOUTS timeouts;
	      // Get the current timeout values.
	      if (GetCommTimeouts(Stream->fd,&timeouts)) {
		// Set the read operations to non-blocking.
		timeouts.ReadIntervalTimeout         = MAXDWORD; 
		timeouts.ReadTotalTimeoutMultiplier  = 0;
		timeouts.ReadTotalTimeoutConstant    = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant   = 0;
		if (SetCommTimeouts(Stream->fd, &timeouts)) {
		  return true;
		}
	      }
	    }
	  }
	}
      }
    }
    Close();
    return false;
#endif
  }
  return false;
}
///

/// SerialStream::Close
// Close the serial stream handle again.
void SerialStream::Close(void)
{
  if (Stream) {
#if defined(__unix__)
    close(Stream->fd);
#elif defined(WIN32)
    if (Stream->fd) {
      if (Stream->readpending || Stream->writepending)
	CancelIo(Stream->fd);
      CloseHandle(Stream->fd);
    }
    if (Stream->readevent) {
      CloseHandle(Stream->readevent);
    }
    if (Stream->writeevent) {
      CloseHandle(Stream->writeevent);
    }
#endif
    delete Stream;
    Stream = 0;
  }
}
///

/// SerialStream::SetBaudRate
// Set the input and output baud rate to the indicated value.
bool SerialStream::SetBaudRate(int rate)
{
  if (Stream) {
#if defined(UNIX)
    struct termios t; 
    speed_t s = B300;
    //
    if (tcgetattr(Stream->fd,&t) == 0) {  
      switch(rate) {
      case 50:
	s = B50;
	break;
      case 75:
	s = B75;
	break;
      case 110:
	s = B110;
	break;
      case 134:
	s = B134;
	break;
      case 150:
	s = B150;
	break;
      case 300:
	s = B300;
	break;
      case 600:
	s = B600;
	break;
      case 1200:
	s = B1200;
	break;
      case 1800:
	s = B1800;
	break;
      case 2400:
	s = B2400;
	break;
      case 4800:
	s = B4800;
	break;
      case 9600:
	s = B9600;
	break;
      case 19200:
	s = B19200;
	break;
      default:
	return false;
      }
      if (cfsetospeed(&t,s) == 0 && cfsetispeed(&t,s) == 0 && tcsetattr(Stream->fd,TCSANOW,&t) == 0)
	return true;  
    }
#elif defined(WIN32)
    DCB dcb = {0};
    // Get the current device status.
    if (GetCommState(Stream->fd, &dcb)) {
      // have device status here, insert the baud
      // rate. Win32 has a couple of baud rates less
      // than *ix.
      switch(rate) {
      case 110:
	dcb.BaudRate = CBR_110;
	break;
      case 300:
	dcb.BaudRate = CBR_300;
	break;
      case 600:
	dcb.BaudRate = CBR_600;
	break;
      case 1200:
	dcb.BaudRate = CBR_1200;
	break;
      case 2400:
	dcb.BaudRate = CBR_2400;
	break;
      case 4800:
	dcb.BaudRate = CBR_4800;
	break;
      case 9600:
	dcb.BaudRate = CBR_9600;
	break;
      case 19200:
	dcb.BaudRate = CBR_19200;
	break;
      default:
	return false;
      }
      if (!SetCommState(Stream->fd, &dcb))
	return false;
      return true;
    }
    return false;
#endif
  }
  return false;
}
///

/// SerialStream::SetStopBits
bool SerialStream::SetStopBits(int bits)
{ 
  if (Stream) {
#if defined(UNIX)
    struct termios t; 
    //
    if (tcgetattr(Stream->fd,&t) == 0) {
      switch(bits) {
      case 1:
	t.c_cflag &= ~CSTOPB;
	break;
      case 2:
	t.c_cflag |= CSTOPB;
	break;
      default:
	return false;
      }
      if (tcsetattr(Stream->fd,TCSANOW,&t) == 0)
	return true;
    }
#elif defined(WIN32)
    DCB dcb = {0};
    // Get the current device status.
    if (GetCommState(Stream->fd, &dcb)) {
      switch(bits) {
      case 1:
	dcb.StopBits = ONESTOPBIT;
	break;
      case 2:
	dcb.StopBits = TWOSTOPBITS;
	break;
      }
      if (!SetCommState(Stream->fd,&dcb)) {
	return false;
      }
      return true;
    }
    return false;
#endif
  }
  return false;
}
///

/// SerialStream::SetDataBits
bool SerialStream::SetDataBits(int bits)
{  
  if (Stream) {
#if defined(UNIX)
    struct termios t; 
    //
    if (tcgetattr(Stream->fd,&t) == 0) {
      t.c_cflag &= ~CSIZE;
      switch (bits) {
      case 5:
	t.c_cflag |= CS5;
	break;
      case 6:
	t.c_cflag |= CS6;
	break;
      case 7:
	t.c_cflag |= CS7;
	break;
      case 8:
	t.c_cflag |= CS8;
	break;
      default:
	return false;
      }
      if (tcsetattr(Stream->fd,TCSANOW,&t) == 0)
	return true;
    }
#elif defined(WIN32)
    DCB dcb = {0};
    // Get the current device status.
    if (GetCommState(Stream->fd, &dcb)) {
      dcb.ByteSize = bits;
      if (!SetCommState(Stream->fd,&dcb)) {
	return false;
      }
      return true;
    }
    return false;
#endif
  }
  return false;
}
///

/// SerialStream::SetHardwareHandshake
// Enable or disable the hardware handshake by CTS/RTS
// default is off.
bool SerialStream::SetHardwareHandshake(bool onoff)
{  
  if (Stream) {
#if defined(UNIX)
    struct termios t; 
    //
    if (tcgetattr(Stream->fd,&t) == 0) { 
#ifdef HAS_CRTSCTS_DEFINE 
      if (onoff) {
	t.c_cflag |= CRTSCTS;
      } else {
	t.c_cflag &= ~CRTSCTS;
      }
#endif
      if (tcsetattr(Stream->fd,TCSANOW,&t) == 0)
	return true;
    }
#elif defined(WIN32)
    DCB dcb = {0};
    // Get the current device status.
    if (GetCommState(Stream->fd, &dcb)) {
      dcb.fOutxCtsFlow = onoff;
      dcb.fOutxDsrFlow = false;
      if (!SetCommState(Stream->fd,&dcb)) {
	return false;
      }
      return true;
    }
    return false;
#endif
  }
  return false;
}
///

/// SerialStream::SetRTSState
bool SerialStream::SetRTSState(bool onoff)
{
  if (Stream) {
#if defined(UNIX)
    return SetRTSLine(Stream->fd,onoff);
#elif defined (WIN32)
    if (!EscapeCommFunction(Stream->fd,(onoff)?SETRTS:CLRRTS))
      return false;
    return true;
#endif
  }
  return false;
}
///

/// SerialStream::SetDTRState
bool SerialStream::SetDTRState(bool onoff)
{
  if (Stream) {
#if defined(UNIX)
    return SetDTRLine(Stream->fd,onoff);
#elif defined (WIN32)
    if (!EscapeCommFunction(Stream->fd,(onoff)?SETDTR:CLRDTR))
      return false;
    return true;
#endif
  }
  return false;
}
///

/// SerialStream::GetCTSState
// Read the status of the CTS line, return in the
// indicated reference, return a true/false success
// indicator.
bool SerialStream::GetCTSState(bool &state)
{
  if (Stream) {
#if defined(UNIX)
    return ReadCTSLine(Stream->fd,state);
#elif defined(WIN32)
   DWORD dwModemStatus;

   if (!GetCommModemStatus(Stream->fd,&dwModemStatus))
     return false;
   
   state = (dwModemStatus & MS_CTS_ON)?true:false;
   return true;
#endif
  }
  return false;
}
///

/// SerialStream::GetDSRState
// Read the status of the DSR line, return in the
// indicated reference, return a true/false success
// indicator.
bool SerialStream::GetDSRState(bool &state)
{
  if (Stream) {
#if defined(UNIX)
    return ReadDSRLine(Stream->fd,state);
#elif defined(WIN32)
   DWORD dwModemStatus;

   if (!GetCommModemStatus(Stream->fd,&dwModemStatus))
     return false;

   state = (dwModemStatus & MS_DSR_ON)?true:false;
   return true;
#endif
  }
  return false;
}
///

/// SerialStream::GetCDState
// Read the status of the CD line, return in the
// indicated reference, return a true/false success
// indicator.
bool SerialStream::GetCDState(bool &state)
{
  if (Stream) {
#if defined(UNIX)
    return ReadCDLine(Stream->fd,state);
#elif defined(WIN32)
   DWORD dwModemStatus;

   if (!GetCommModemStatus(Stream->fd,&dwModemStatus))
     return false;

   state = (dwModemStatus & MS_RLSD_ON)?true:false;
#endif
  }
  return false;
}
///

/// SerialStream::GetFramingErrors
// Read the number of serial framing errors since 
// the last bootstrap. Return false if this value
// could not be obtained.
bool SerialStream::GetFramingErrors(int &cnt)
{
  if (Stream) {
#if defined(UNIX)
    return ReadFramingErrors(Stream->fd,cnt);
#elif defined(WIN32)
    COMSTAT comStat;
    DWORD   dwErrors;
    //
    // Get and clear the current errors on the port.
    if (!ClearCommError(Stream->fd,&dwErrors,&comStat))
      return false;
    
    if (dwErrors & CE_FRAME)
      framingerrors++;
    if (dwErrors & CE_OVERRUN)
      portoverruns++;
    if (dwErrors & CE_RXPARITY)
      parityerrors++;
    if (dwErrors & CE_RXOVER)
      bufferoverruns++;
    
    cnt = framingerrors;
    return true;
#endif
  }
  return false;
}
///

/// SerialStream::GetPortOverrunErrors
bool SerialStream::GetPortOverrunErrors(int &cnt)
{
  if (Stream) {
#if defined(UNIX)
    return ReadByteOverrunErrors(Stream->fd,cnt);
#elif defined(WIN32)
    COMSTAT comStat;
    DWORD   dwErrors;
    //
    // Get and clear the current errors on the port.
    if (!ClearCommError(Stream->fd,&dwErrors,&comStat))
      return false;
    
    if (dwErrors & CE_FRAME)
      framingerrors++;
    if (dwErrors & CE_OVERRUN)
      portoverruns++;
    if (dwErrors & CE_RXPARITY)
      parityerrors++;
    if (dwErrors & CE_RXOVER)
      bufferoverruns++;
    
    cnt = portoverruns;
    return true;
#endif
  }
  return false;
}
///

/// SerialStream::GetParityErrors
bool SerialStream::GetParityErrors(int &cnt)
{
  if (Stream) {
#if defined(UNIX)
    return ReadParityErrors(Stream->fd,cnt);
#elif defined(WIN32)
    COMSTAT comStat;
    DWORD   dwErrors;
    //
    // Get and clear the current errors on the port.
    if (!ClearCommError(Stream->fd,&dwErrors,&comStat))
      return false;
    
    if (dwErrors & CE_FRAME)
      framingerrors++;
    if (dwErrors & CE_OVERRUN)
      portoverruns++;
    if (dwErrors & CE_RXPARITY)
      parityerrors++;
    if (dwErrors & CE_RXOVER)
      bufferoverruns++;
    
    cnt = parityerrors;
    return true;
#endif
  }
  return false;
}
///

/// SerialStream::GetBufferOverrunErrors
bool SerialStream::GetBufferOverrunErrors(int &cnt)
{
  if (Stream) {
#if defined(UNIX)
    return ReadBufferOverrunErrors(Stream->fd,cnt);
#elif defined(WIN32)
    COMSTAT comStat;
    DWORD   dwErrors;
    //
    // Get and clear the current errors on the port.
    if (!ClearCommError(Stream->fd,&dwErrors,&comStat))
      return false;
    
    if (dwErrors & CE_FRAME)
      framingerrors++;
    if (dwErrors & CE_OVERRUN)
      portoverruns++;
    if (dwErrors & CE_RXPARITY)
      parityerrors++;
    if (dwErrors & CE_RXOVER)
      bufferoverruns++;
    
    cnt = bufferoverruns;
    return true;
#endif
  }
  return false;
}
///

/// SerialStream::Read
// Read data from a serial stream, return the
// number of bytes read
long SerialStream::Read(unsigned char *buffer,long size)
{
  if (Stream) {
#if defined(UNIX)
    return read(Stream->fd,buffer,size);
#elif defined(WIN32)
    long totalbytes = 0; // total number of bytes read.
    while(totalbytes < size) {
      // Check whether there are still any bytes in the input
      // buffer waiting to be retrieved. If so, do this first.
      if (Stream->nextavail < Stream->lastavail) {
	size_t read;
	read = Stream->lastavail - Stream->nextavail;
	if (read > size_t(size))
	  read = size;
	memcpy(buffer,Stream->nextavail,read);
	buffer             += read;
	Stream->nextavail  += read;
	totalbytes         += long(read);
      } else {
	DWORD readbytes = 0;
	// No bytes in the buffer. Ok, need to launch a new read request.
	if (!Stream->readpending) {
	  long chunksize = size - totalbytes; // try to fulfill the request at once
	  if (chunksize > sizeof(Stream->inputbuffer))
	    chunksize = sizeof(Stream->inputbuffer);
	  // No read has been launched yet. So do now. Reset the buffer
	  // start from scratch.
	  Stream->nextavail = Stream->lastavail = Stream->inputbuffer;
	  if (!ReadFile(Stream->fd,Stream->inputbuffer,chunksize,&readbytes,&Stream->osreader)) {
	    if (GetLastError() != ERROR_IO_PENDING) {
	      // Communications error, return with an error code immediately.
	      return -1;
	    } else {
	      // The read is now on the way for data to arrive at the input
	      // port.
	      Stream->readpending = true;
	    }
	  } else {
	    // Read completed. Set the last byte, and continue with the
	    // loop to provide user data.
	    Stream->lastavail = Stream->inputbuffer + readbytes;
	    // If no bytes are available, stop here
	    if (readbytes == 0)
	      return totalbytes;
	  }
	} else {
	  // Read has been started already. Check whether it returned. Do
	  // not wait at all.
	  switch(WaitForSingleObject(Stream->osreader.hEvent,0)) {
	  case WAIT_OBJECT_0:
	    Stream->readpending = false;
	    if (!GetOverlappedResult(Stream->fd, &Stream->osreader, &readbytes, FALSE)) {
	      // Communications error
	      return -1;
	    } else {
	      // Read returned sucessfully. Set the last byte in the input
	      // buffer and loop again to fulfill the user request.
	      Stream->lastavail = Stream->inputbuffer + readbytes;
	      continue;
	    }
	    break;
	  case WAIT_TIMEOUT:
	    // Not yet ready. Don't wait any longer, just return what
	    // we have read so far.
	    return totalbytes;
	  default:
	    // Error in the WaitForSingleObject; abort.
	    // This indicates a problem with the OVERLAPPED structure's
	    // event handle.
	    return -1;
	  } /* of switch */
	} /* of read started */
      } /* of bytes available, else part */
    } /* of read-loop */
    return totalbytes;
#endif
  } else return -1;
  return 0;
}
///

/// SerialStream::Write
// Write data to a serial stream, return the number
// of bytes written.
long SerialStream::Write(const unsigned char *buffer,long size)
{
  if (Stream) {
#if defined(__unix)
    return write(Stream->fd,buffer,size);
#elif defined(WIN32)
    long total = 0;
    while(size) {
      DWORD byteswritten = 0;
      long chunksize;
      // First check whether the output buffer is available. If not
      // we must wait for the previous write to complete.
      if (Stream->writepending) {
	switch(WaitForSingleObject(Stream->oswriter.hEvent, INFINITE)) {
	case WAIT_OBJECT_0:
	  // Data write returned now.
	  Stream->writepending = false;
	  if (!GetOverlappedResult(Stream->fd,&Stream->oswriter,&byteswritten,false)) {
	    // Last writing failed, signal an error.
	    return -1;
	  }
	  break;
	default:
	  // An error in WaitForSingleObject.
	  return -1;
	}
      } else {
	// Check how many bytes we can start writing at once without
	// having to wait for them to get written out.
	chunksize = size;
	if (chunksize > sizeof(Stream->outputbuffer))
	  chunksize = sizeof(Stream->outputbuffer);
	// Setup an overlapped write for the given number of bytes, signal
	// success, return immediately, do not wait for completion in here.
	memcpy(Stream->outputbuffer,buffer,chunksize);
	buffer += chunksize;
	size   -= chunksize;
	total  += chunksize;
	// Launch the write operation
	if (!WriteFile(Stream->fd,Stream->outputbuffer,chunksize,&byteswritten,&Stream->oswriter)) {
	  if (GetLastError() != ERROR_IO_PENDING) {
	    // Write operation failed for unknown reason.
	    return -1;
	  } else {
	    // Write is now on the way, we need to wait for completion.
	    Stream->writepending = true;
	  }
	} // Write returned immediately.
      } /* of if write pending, else part */
    } /* of looping over buffer */
    return total;
#endif
  } else return -1;
  return 0;
}
///

/// SerialStream::Flush
// Flush the current output buffer, dispose its
// contents, and do not set it.
void SerialStream::Flush(void)
{
  if (Stream) {
#if defined(UNIX)
    tcflush(Stream->fd,TCIOFLUSH);
#elif defined(WIN32)
    if (Stream->writepending) {
      CancelIo(Stream->fd);
      Stream->writepending = false;
    }
#endif
  }
}
///

/// SerialStream::Drain
// Wait until the output buffer is empty, then
// return. Or return an error.
bool SerialStream::Drain(void)
{
  if (Stream) {
#if defined(UNIX)
    return DrainSerialOutputBuffer(Stream->fd);
#elif defined(WIN32)
    if (Stream->writepending) {
      DWORD byteswritten;
      switch(WaitForSingleObject(Stream->oswriter.hEvent, INFINITE)) {
      case WAIT_OBJECT_0:
	// Data write returned now.
	Stream->writepending = false;
	if (!GetOverlappedResult(Stream->fd,&Stream->oswriter,&byteswritten,false)) {
	  // Last writing failed, signal an error.
	  return false;
	}
	break;
      default:
	// An error in WaitForSingleObject.
	return false;
      }
    }
    return true;
#endif
  }
  return false;
}
///
