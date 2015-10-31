/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: termios.cpp,v 1.6 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Low-level termios compatibility wrapper
 ** for low-level serial device handling.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "unistd.hpp"
#if HAVE_FCNTL_H
# if HAVE_SYS_IOCTL_H && HAVE_LINUX_SERIAL_H
extern "C" {
#  include <sys/ioctl.h>
#  include <linux/serial.h>
}
// Check for availibility of the modem control lines
// get and set
#  if defined(TIOCMGET) && defined(TIOCMSET) && defined(TIOCM_RTS) && defined(TIOCM_CTS) && defined(TIOCM_DSR) && defined(TIOCM_DTR) && defined(TIOCM_CD)
#   define USE_MODEM_LINES
#  endif
#  if defined(TIOCGICOUNT)
#   define USE_ICOUNTER_STATUS
#  endif
# endif
# if HAVE_TERMIOS_H
extern "C" {
#  include <termios.h>
}
# endif
#endif
///

/// SetDTRLine
// Set the DTR line to the indicated state. Returns a boolean
// success/failure indicator
bool SetDTRLine(int Stream,bool state)
{
#ifdef USE_MODEM_LINES
  if (Stream >= 0) {
    int lines = 0;
    //
    // First try to read the modem lines.
    if (ioctl(Stream,TIOCMGET,&lines) == 0) {
      // Now set the DTR and RTS lines as requested.
      if (state) {
	lines |= TIOCM_DTR;
      } else {
	lines &= ~TIOCM_DTR;
      }
      if (ioctl(Stream,TIOCMSET,&lines) == 0) {
	return true;
      }
    }
  }
#endif
  return false;
}
///

/// SetRTSLine
// Set the RTS line to the indicate state. Returns a boolean
// success/failure indicator
bool SetRTSLine(int Stream,bool state)
{
#ifdef USE_MODEM_LINES
  if (Stream >= 0) {
    int lines = 0;
    //
    // First try to read the modem lines.
    if (ioctl(Stream,TIOCMGET,&lines) == 0) {
      // Now set the DTR and RTS lines as requested.
      if (state) {
	lines |= TIOCM_RTS;
      } else {
	lines &= ~TIOCM_RTS;
      }
      if (ioctl(Stream,TIOCMSET,&lines) == 0) {
	return true;
      }
    }
  }
#endif
  return false;
}
///

/// ReadModemLines
// Read the contents of the serial modem lines CTS,DSR and CD
// and returns them, ditto a boolean failure indicator.
bool ReadModemLines(int Stream,bool &cts,bool &dsr,bool &cd)
{
#ifdef USE_MODEM_LINES    
  int lines = 0;
  if (ioctl(Stream,TIOCMGET,&lines) == 0) {
    // Insert the new states of the lines into
    // the backup/history registers.
    dsr = (lines & TIOCM_DSR)?true:false;
    cts = (lines & TIOCM_CTS)?true:false;
    cd  = (lines & TIOCM_CD)?true:false;
    return true;
  }
#endif
  return false;
}
///

/// ReadCTSLine
bool ReadCTSLine(int Stream,bool &cts)
{
#ifdef USE_MODEM_LINES    
  int lines = 0;
  if (ioctl(Stream,TIOCMGET,&lines) == 0) {
    cts = (lines & TIOCM_CTS)?true:false;
    return true;
  }
#endif
  return false;
}
///

/// ReadDSRLine
bool ReadDSRLine(int Stream,bool &cts)
{
#ifdef USE_MODEM_LINES    
  int lines = 0;
  if (ioctl(Stream,TIOCMGET,&lines) == 0) {
    cts = (lines & TIOCM_DSR)?true:false;
    return true;
  }
#endif
  return false;
}
///

/// ReadCDLine
bool ReadCDLine(int Stream,bool &cts)
{
#ifdef USE_MODEM_LINES    
  int lines = 0;
  if (ioctl(Stream,TIOCMGET,&lines) == 0) {
    cts = (lines & TIOCM_CD)?true:false;
    return true;
  }
#endif
  return false;
}
///

/// ReadErrorCounters
// Provide the error counters for overrun,framing and parity
// errors. Return a success/failure indicator whether the
// errors could be read.
bool ReadErrorCounters(int stream,int &framing,int &byteoverrun,int &parity,int &bufferoverrun)
{
#ifdef USE_ICOUNTER_STATUS
  if (stream >= 0) {
    struct serial_icounter_struct sis; // will contain the counters.
    //
    // Now run the ioctl on this to copy the
    // icounter structure over.
    if (ioctl(stream,TIOCGICOUNT,&sis) == 0) {
      // Worked. Deliver the counters.
      framing       = sis.frame;
      byteoverrun   = sis.overrun;
      parity        = sis.parity;
      bufferoverrun = sis.buf_overrun;
      return true;
    }
  }
#endif
  return false;
}
///

/// ReadFramingErrors
// Return the number of framing errors since bootstrap
bool ReadFramingErrors(int stream,int &framing)
{
#ifdef USE_ICOUNTER_STATUS
  if (stream >= 0) {
    struct serial_icounter_struct sis; // will contain the counters.
    //
    // Now run the ioctl on this to copy the
    // icounter structure over.
    if (ioctl(stream,TIOCGICOUNT,&sis) == 0) {
      // Worked. Deliver the counters.
      framing       = sis.frame;
      return true;
    }
  }
#endif
  return false;
}
///

/// ReadByteOverrunErrors
// Return the number of byte input buffer overrun errors since bootstrap
bool ReadByteOverrunErrors(int stream,int &byteoverrun)
{
#ifdef USE_ICOUNTER_STATUS
  if (stream >= 0) {
    struct serial_icounter_struct sis; // will contain the counters.
    //
    // Now run the ioctl on this to copy the
    // icounter structure over.
    if (ioctl(stream,TIOCGICOUNT,&sis) == 0) {
      // Worked. Deliver the counters.
      byteoverrun   = sis.overrun;
      return true;
    }
  }
#endif
  return false;
}
///

/// ReadParityErrors
// Return the number of parity errors since bootstrap
bool ReadParityErrors(int stream,int &parity)
{
#ifdef USE_ICOUNTER_STATUS
  if (stream >= 0) {
    struct serial_icounter_struct sis; // will contain the counters.
    //
    // Now run the ioctl on this to copy the
    // icounter structure over.
    if (ioctl(stream,TIOCGICOUNT,&sis) == 0) {
      // Worked. Deliver the counters.
      parity        = sis.parity;
      return true;
    }
  }
#endif
  return false;
}
///

/// ReadBufferOverrunErrors
// Return the number of buffer overruns since bootstrap
bool ReadBufferOverrunErrors(int stream,int &bufferoverrun)
{
#ifdef USE_ICOUNTER_STATUS
  if (stream >= 0) {
    struct serial_icounter_struct sis; // will contain the counters.
    //
    // Now run the ioctl on this to copy the
    // icounter structure over.
    if (ioctl(stream,TIOCGICOUNT,&sis) == 0) {
      // Worked. Deliver the counters.
      bufferoverrun = sis.buf_overrun;
      return true;
    }
  }
#endif
  return false;
}
///

/// DrainSerialOutputBuffer
// Wait until the output buffer is empty, then
// return. Or return an error.
bool DrainSerialOutputBuffer(int stream)
{
#ifdef USE_ICOUNTER_STATUS
  unsigned int lsr;
  //
  // now wait until the transmitter holding register has
  // been cleared
  do {
    if (ioctl(stream, TIOCSERGETLSR, &lsr) < 0) {
      return false;
    }
    // Argl, busy waiting...
  } while ((lsr & TIOCSER_TEMT) == 0);
#endif
  return true;
}
///
