/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: serialstream.hpp,v 1.5 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: An interface abstraction for serial ports,
 ** one level above the operating system. This here grands access
 ** to all major functions of a serial device, including input/output
 ** and control of the serial lines.
 **********************************************************************************/

#ifndef SERIALSTREAM_HPP
#define SERIALSTREAM_HPP

/// Includes
///

/// class SerialStream
class SerialStream {
  //
  // SerialHandle
  struct SerialHandle;
  // Os dependent serial stream handle, to be
  // defined inside, as a pImpl.
  struct SerialHandle *Stream;
  //
  //
public:
  // Construct a serial stream, leave it unused.
  SerialStream(void);
  ~SerialStream(void);
  //
  // Suggest a name for the serial connection, to
  // be used for the open path.
  static const char *SuggestName(void);
  //
  // Open a serial stream for reading and writing, return
  // a success/failure indicator.
  bool Open(const char *path);
  // Shutdown the stream.
  void Close(void);
  // Check whether we are open
  bool isOpen(void) const
  {
    return (Stream)?true:false;
  }
  //
  // Write the indicated number of bytes over the stream, return the
  // number of bytes written, or -1 on error.
  long Write(const unsigned char *buffer,long size);
  //
  // Read the indicated number of bytes from the buffer, return
  // -1 on error, 0 on EOF.
  long Read(unsigned char *buffer,long size);
  //
  // Set the input and output baud rate to the indicated value.
  bool SetBaudRate(int rate);
  //
  // Set the number of data bits in the stream.
  bool SetStopBits(int bits);
  //
  // Set the number of data bits
  bool SetDataBits(int bits);
  //
  // Enable or disable the hardware handshake by CTS/RTS
  // default is off.
  bool SetHardwareHandshake(bool onoff);
  //
  // Set the status of the RTS line
  bool SetRTSState(bool onoff);
  //
  // Set the status of the DTR line
  bool SetDTRState(bool onoff);
  //
  // Read the status of the CTS line, return in the
  // indicated reference, return a true/false success
  // indicator.
  bool GetCTSState(bool &state);
  //
  // Read the status of the DSR line, return in the
  // indicated reference, return a true/false success
  // indicator.
  bool GetDSRState(bool &state);
  //
  // Read the status of the CD line, return in the
  // indicated reference, return a true/false success
  // indicator.
  bool GetCDState(bool &state);
  //
  // Read the number of serial framing errors since 
  // the last bootstrap. Return false if this value
  // could not be obtained.
  bool GetFramingErrors(int &cnt);
  //
  // Read the number of serial input port overrun
  // errors since last bootstrap. Return false on
  // error.
  bool GetPortOverrunErrors(int &cnt);
  //
  // Read the number of parity erors since the last
  // bootstrap. Return false on error.
  bool GetParityErrors(int &cnt);
  //
  // Read the number of serial driver buffer overruns,
  // return false if this number cannot be obtained.
  bool GetBufferOverrunErrors(int &cnt);
  //
  // Flush the current output buffer, dispose its
  // contents, and do not set it.
  void Flush(void);
  //
  // Wait until the output buffer is empty, then
  // return. Or return an error.
  bool Drain(void);
};
///

///
#endif
  
