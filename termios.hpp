/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: termios.hpp,v 1.5 2015/05/21 18:52:43 thor Exp $
 ** 
 ** In this module: Low-level termios compatibility wrapper
 ** for low-level serial device handling.
 **********************************************************************************/

#ifndef TERMIOS_HPP
#define TERMIOS_HPP

/// SetDTRLine
// Set the DTR line to the indicated state. Returns a boolean
// success/failure indicator
bool SetDTRLine(int Stream,bool state);
///

/// SetRTSLine
// Set the RTS line to the indicate state. Returns a boolean
// success/failure indicator
bool SetRTSLine(int Stream,bool state);
///

/// ReadModemLines
// Read the contents of the serial modem lines CTS,DSR and CD
// and returns them, ditto a boolean failure indicator.
bool ReadModemLines(int Stream,bool &cts,bool &dsr,bool &cd);
///

/// ReadCTSLine
// Read only the status of the CTS line
bool ReadCTSLine(int Stream,bool &cts);
///

/// ReadDSRLine
// Read only the status of the DSR line
bool ReadDSRLine(int Stream,bool &dsr);
///

/// ReadCDLine
bool ReadCDLine(int Stream,bool &cd);
///

/// ReadErrorCounters
// Provide the error counters for overrun,framing and parity
// errors. Return a success/failure indicator whether the
// errors could be read.
bool ReadErrorCounters(int stream,int &framing,int &byteoverrun,int &parity,int &bufferoverrun);
///

/// ReadFramingErrors
// Return the number of framing errors since bootstrap
bool ReadFramingErrors(int stream,int &framing);
///

/// ReadByteOverrunErrors
// Return the number of byte input buffer overrun errors since bootstrap
bool ReadByteOverrunErrors(int stream,int &byteoverrun);
///

/// ReadParityErrors
// Return the number of parity errors since bootstrap
bool ReadParityErrors(int stream,int &parity);
///

/// ReadBufferOverrunErrors
// Return the number of buffer overruns since bootstrap
bool ReadBufferOverrunErrors(int stream,int &bufferoverrun);
///

/// DrainSerialOutputBuffer
// Wait until the output buffer is empty, then
// return. Or return an error.
bool DrainSerialOutputBuffer(int stream);
///

///
#endif
