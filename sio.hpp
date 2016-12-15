/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sio.hpp,v 1.29 2016/12/04 17:30:54 thor Exp $
 **
 ** In this module: Generic emulation core for all kinds of serial hardware
 ** like printers or disk drives.
 **********************************************************************************/

#ifndef SIO_HPP
#define SIO_HPP

/// Includes
#include "types.hpp"
#include "machine.hpp"
#include "argparser.hpp"
#include "configurable.hpp"
#include "chip.hpp"
///

/// Forward declarations
class Monitor;
class DiskDrive;
class Printer;
class SerialDevice;
class Pokey;
class Timer;
///

/// Class SIO
// This class is the generic emulation layer for any kind
// of serial hardware that can be connected to the serial
// port of the atari computers.
// 
// This is not precisely speaking a chip, but we still 
// require coldstart and warmstart methods
class SIO : public Chip {
  //
  //
public:
  // The type of command, identified by the device
  enum CommandType {
    Off,                     // command does not respond at all (device is off)
    InvalidCommand,          // command not valid
    ReadCommand,             // read from device into system
    WriteCommand,            // write from system into device
    StatusCommand,           // command does not return data nor requires data
    FormatCommand            // special timing for format
  };
  //
  // The regular 19200 baud speed
  enum SpeedType {
    Baud19200 = 47           // note that this is in cycles, not in pokey timer values
  };
  //
  //
private:
  //
  // List of all registered devices we're able to handle.
  List<SerialDevice> Devices;
  //
  //
  class Pokey *pokey;
  //
  // Definition of the state machine of the SIO interface
  enum SIOStateType {
    NoState,               // not in a command frame
    ResetState,            // SIO starts with this on reset
    CmdState,              // command frame active
    AcknowledgeState,      // acknowledge from the command frame
    StatusState,           // read back status from data frame
    ReadState,             // transfering data from the device into the system
    WriteState,            // transfering data from the system into the device
    FlushState,            // SIO waits for a write frame to come back
    DoneState              // Write is completed, send acknowledge
  }                   SIOState; // the state we are currently in
  //
  // Store the command frame here
  UBYTE               CommandFrame[4+1]; // commands for SIO devices
  UBYTE               StatusFrame[2];    // return status information
  UBYTE              *DataFrame;         // input buffer for data
  // Size of the allocated data frame buffer
  int                 DataFrameSize;
  //
  // Index into the command frame, data frame, and number of expected bytes
  int                 CommandFrameIdx;
  int                 DataFrameIdx;
  int                 DataFrameLength;   // length of the expected data frame
  int                 ExpectedBytes;
  CommandType         CmdType;           // type of the currently active command
  //
  // Status of the last interpreted command frame to be read back by Pokey
  UBYTE               CommandStatus;
  // Checksum computed so far
  UBYTE               CurrentSum;
  //
  // The serial transfer speed as Pokey divisor as transmitted by the external
  // device when reading data.
  UWORD               InputSpeed;
  //
  // Flag whether the tape motor is running (or enabled to be running).
  bool                MotorEnabled;
  //
  // The following is true in case we have been warned already.
  bool                HaveWarned;
  //
  // Pointers to the currently acting serial device
  class SerialDevice *ActiveDevice;
  //
  // SIO preferences
  //
  LONG                SerIn_Cmd_Delay;  // typical duration of a command frame reaction
  LONG                Write_Done_Delay; // typical delay to accept a write frame
  LONG                Read_Done_Delay;  // typical time to complete a read command
  LONG                Format_Done_Delay;// typical time to complete a format command
  LONG                Read_Deliver_Delay; // delay from the ok of the read to the delivery of the data
  //
  //
  // Miscellaneous internal methods
  //
  // Given a command frame, identify the device responsible to handle
  // this frame, or NULL if the device is not mounted.
  class SerialDevice *FindDevice(UBYTE commandframe[4]);
  //
  // Ensure the dataframe is large enough for the requested
  // number of bytes, realloc otherwise.
  void ReallocDataFrame(int framesize);
  //
public:
  // constructors and destructors
  SIO(class Machine *mach);
  ~SIO(void);
  //
  // Compute a checksum over a sequence of bytes in the SIO way.
  UBYTE ChkSum(const UBYTE *buffer,int bytes);
  //
  // Register a serial device for handling with SIO.
  void RegisterDevice(class SerialDevice *);
  //
  // Warmstart and ColdStart.
  virtual void WarmStart(void);
  virtual void ColdStart(void);
  //
  // Entry points for Pokey, PIA chipset emulation
  // Write a byte to the serial line
  void WriteByte(UBYTE val);
  //
  // Request more input bytes from SIO by a running
  // command.
  void RequestInput(void);
  //
  // Toggle the command frame on/off:
  // This enables or disables the COMMAND
  // line on the serial port to indicate that
  // a command frame is on its way.
  void SetCommandLine(bool onoff);
  //
  //
  // Bypass the serial overhead for the SIO patch and issues the
  // command directly. It returns a status
  // indicator similar to the ROM SIO call.
  UBYTE RunSIOCommand(UBYTE device,UBYTE unit,UBYTE command,
		      ADR Mem,UWORD size,UWORD aux,UBYTE timeout);
  //
  // Private to the above: Delay for the given timer until the
  // serial device reacts, and run thru the VBI. Returns true in
  // case we must abort the operation.
  bool WaitForSerialDevice(class Timer *time,ULONG &timecount);
  //
  // The following two functions are for the rather exotic "concurrent mode"
  // serial transfer handling of the 850 interface box. These two
  // bypass the usual SIO operation and take over the serial port directly,
  // with the clocking coming from the 850.
  //
  // Test whether a serial byte from concurrent mode is available. Return
  // true if so, and return the byte.
  bool ConcurrentRead(UBYTE &data);
  //
  // Output a serial byte thru concurrent mode over the channel.
  void ConcurrentWrite(UBYTE data);
  //
  //
  // The following two functions support the tape-only communications.
  // This is enabled by Pokey only in case the motor line of PIA is
  // enabled.
  //
  // Enable or disable the state of the tape motor,
  // thus potentially running or stopping the
  // motor. The argument is the raw state of the PIA pin, which
  // is active-low.
  void SetMotorLine(bool onoff);
  //
  // Output a byte from the tape recorder.
  void TapeWrite(UBYTE data);
  //
  // Get the status of the motor. If the returned value is true,
  // the tape motor may run.
  bool isMotorEnabled(void) const
  {
    return MotorEnabled;
  }
  //
  // Print the status of the chip over the monitor
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Implement the functions of the configurable class
  virtual void ParseArgs(class ArgParser *args);
};
///

///
#endif
