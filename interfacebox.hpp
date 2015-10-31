/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: interfacebox.hpp,v 1.14 2014/03/16 14:54:08 thor Exp $
 **
 ** In this module: Emulation of the 850 interface box.
 **********************************************************************************/

#ifndef INTERFACEBOX_HPP
#define INTERFACEBOX_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "serialdevice.hpp"
#include "saveable.hpp"
#include "serialstream.hpp"
///

/// Class InterfaceBox
// This class emulates the 850 or 850 XL interface box. Or rather,
// a partial simplification of it that a) supports only one channel
// similar to the 850XL, and b) does not have a printer-support
// since that's handled by the printer interface anyhow.
class InterfaceBox : public SerialDevice, public Saveable {
  //
  // The stream that is connected to the interface
  class SerialStream   *SerialStream;
  //
  // Name of the device we output the data to. This is the name of
  // the game.
  char                 *DeviceName;
  //
  // The following boolean is set in case the interface box
  // is "on".
  bool                  BoxOn;
  //
  // 
  // Serial parameters that can be set/read
  //
  // Requested output level of the DTR line
  bool                  DTRState;
  // Requested output level of the RTS line
  bool                  RTSState;
  // Requested output level of the XMT (TxD) line
  bool                  XMTState;
  // Test whether handshaking by DSR is requested
  bool                  DSRHandshake;
  // Test whether handshaking by CTS is requested
  bool                  CTSHandshake;
  // Test whether handshaking by CRX (RxD) is requested
  bool                  CRXHandshake;
  // State of some modem lines, history for the status cmd.
  // Old state of the DSR line.
  bool                  DSRState;
  // Old state of the CTS line.
  bool                  CTSState;
  // Old state of the CD line.
  bool                  CDState;
  // Requested number of stop bits (1 1/2 is not accepted)
  int                   StopBits;
  // Requested number of data bits
  int                   DataBits;
  // Baud rate (literally, must be translated back and forth)
  int                   BaudRate;
  // The parity is not in our hands. This is because the
  // interface software keeps care of it.
  // Various error flags:
  // Framing error
  bool                  FramingError;
  // Lost a byte by not reading fast enough
  bool                  ByteOverrun;
  // Parity error detected
  bool                  ParityError;
  // Input buffer overrun
  bool                  BufferOverrun;
  //
  // Last found counters for the overrun cases. This will
  // be compared with the latest errors from the serial_icounter_struct
  // and possibly updates the above errors.
  int                   FramingErrorCnt;
  int                   ByteOverrunCnt;
  int                   ParityErrorCnt;
  int                   BufferOverrunCnt;
  //
  // The following bool is set in case
  // we shall not try to re-open the serial because we
  // know its broken.
  bool                  DevError;
  //
  // The following bool is set if we are in concurrent mode
  bool                  ConcurrentActive;
  //  
  //
  // Open the serial device file descriptor
  // and install the parameters. Must get
  // called before using the file descriptor.
  void OpenChannel(void);
  //
  // Signal a device error and close the channel,
  // first sending the warning text to the user
  void SignalDeviceError(const char *msg);
  //
  // By using termios calls, install or modify
  // the requested parameters.
  void InstallParameters(void);
  //
  // Install the requested parameters of the modem
  // lines if possible. Not all systems may support
  // this, though.
  void SetModemLines(void);
  //
  // Monitor the selected modem lines and return
  // false in case they are not set.
  bool MonitorModemLines(void);
  //
  // Update the error state flags
  void UpdateErrors(void);
  //
  // Read the pokey settings for entering the
  // concurrent mode, check the modem control
  // lines for proper settings and enter the
  // concurrent mode.
  UBYTE ReadPokeyStatus(UBYTE *buffer);
  //
  // Send data out to the serial stream
  // Returns an error indicator, possibly.
  UBYTE SendData(const UBYTE *data,int size);
  //
  // Set the baud rate and lines to monitor
  // Returns a status indicator.
  UBYTE SetBaudRate(UBYTE aux1,UBYTE aux2);
  //
  // Set the DTR and RTS lines. This command also
  // works only in concurrent mode off and
  // returns the status code for SIO.
  UBYTE SetDTR(UBYTE aux);
  //
  // Read the two status bytes (only in non-concurrent mode)
  // and return them.
  UBYTE ReadStatusLines(UBYTE *buffer);
  //
  // Read the boot parameters for the 850 interface
  // and place them into the buffer. This returns
  // the twelve bytes to be placed into the DCB for
  // booting the boot code.
  UBYTE ReadDCB(UBYTE *buffer);
  //
  // Provide the boot code of the 850 interface
  UBYTE ReadBootCode(UBYTE *buffer);
  //
  // This bootstraps the handler main code
  // which gets relocated by the boot code
  UBYTE ReadHandler(UBYTE *buffer);
  //
public:
  // Constructor and destructor
  InterfaceBox(class Machine *mach);
  ~InterfaceBox(void);
  //
  // The following methods come from the SerialDevice interface
  // and have to be implemented in order to make this work:
  // Check whether this device accepts the indicated command
  // as valid command, and return the command type of it.
  virtual SIO::CommandType CheckCommandFrame(const UBYTE *CommandFrame,int &datasize,
					     UWORD speed);
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
  virtual UBYTE ReadStatus(const UBYTE *CommandFrame,UWORD &delay,UWORD &speed);
  //
  // Rather exotic concurrent read/write commands. These methods are used for
  // the 850 interface box to send/receive bytes when bypassing the SIO. In these
  // modes, clocking comes from the external 850 interface, and SIO remains unactive.
  //
  // Check whether we have a concurrent byte to deliver. Return true and the byte
  // if so. Return false otherwise.
  virtual bool ConcurrentRead(UBYTE &data);
    //
  // Check whether this device is able to accept a concurrently written byte. 
  // Return true if so, and then deliver the byte to the backend of this device.
  // Otherwise, return false.
  virtual bool ConcurrentWrite(UBYTE data);
  //
  // Drain the buffer, i.e. wait until all its data has been written out.
  // Returns false on error.
  bool Drain(void);
  //
  // Other methods imported by the SerialDevice class:
  //
  // ColdStart and WarmStart
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Methods inherited from the Saveable class.
  //
  // Read and write the state into a snapshot class
  // This is the main purpose why we are here. This method
  // should read the state from a snapshot, providing the current
  // settings as defaults. Hence, we can also use it to save the
  // configuration by putting the defaults into a file.
  virtual void State(class SnapShot *snap);
  //
  // Methods inherited from the Configurable class.
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
