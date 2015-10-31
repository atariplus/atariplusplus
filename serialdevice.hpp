/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: serialdevice.hpp,v 1.18 2014/03/16 14:54:08 thor Exp $
 **
 ** In this module: Interface specifications for serial devices.
 ** All serial devices (printer, disks, ...) must be derived from
 ** this class
 **********************************************************************************/

#ifndef SERIALDEVICE_HPP
#define SERIALDEVICE_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "chip.hpp"
#include "machine.hpp"
#include "sio.hpp"
///

/// Class SerialDevice
// This class defines the generic interface to all serial devices
class SerialDevice : public Chip, public Node<class SerialDevice> {
  //
  // The following Id is checked against the command frame byte
  // in the SIO routine. If it fits, then this define is fine.
  UBYTE DeviceId;
  //
public:
  // Constructors and destructors
  SerialDevice(class Machine *mach,const char *name,UBYTE id)
    : Chip(mach,name), DeviceId(id)
  { }
  //
  virtual ~SerialDevice(void)
  { }
  //
  // Overloading of NextOf/PrevOf to walk thru the list of serial devs and
  // not thru the list of chips if we have a SerialDevice.
  class SerialDevice *NextOf(void) const
  {
    return Node<SerialDevice>::NextOf();
  }
  class SerialDevice *PrevOf(void) const
  {
    return Node<SerialDevice>::PrevOf();
  }
  //
  // The following methods are requiered by SIO:
  //
  // Check whether this device is responsible for the indicated command frame
  virtual bool HandlesFrame(const UBYTE *commandframe)
  {
    return (commandframe[0] == DeviceId);
  }
  //
  // Check whether this device accepts the indicated command
  // as valid command, and return the command type of it.
  // As secondary argument, it also returns the number of bytes 
  // in the data frame (if there is any). If this is a write
  // command, datasize can be set to zero to indicate single
  // byte transfer.
  virtual SIO::CommandType CheckCommandFrame(const UBYTE *CommandFrame,int &datasize,UWORD speed) = 0;
  //
  // Acknowledge the command frame. This is called as soon the SIO implementation
  // in the host system tries to receive the acknowledge function from the
  // client. Will return 'A' in case the command frame is accepted. Note that this
  // is only called if CheckCommandFrame indicates already a valid command.
  virtual UBYTE AcknowledgeCommandFrame(const UBYTE *,UWORD &,UWORD &)
  {
    // Default is to rely that SIO does the right thing here.
    return 'A';
  }
  //
  // Read bytes from the device into the system. Returns the command status
  // after the read operation, and installs the number of bytes really written
  // into the data size if it differs from the requested amount of bytes.
  // SIO will call back in case only a part of the buffer has been transmitted.
  // Delay is the number of 15kHz cycles (lines) the command requires for completion.
  virtual UBYTE ReadBuffer(const UBYTE *CommandFrame,UBYTE *buffer,
			   int &datasize,UWORD &delay,UWORD &speed) = 0;
  //  
  // Write the indicated data buffer out to the target device.
  // Return 'C' if this worked fine, 'E' on error. 
  virtual UBYTE WriteBuffer(const UBYTE *CommandFrame,const UBYTE *buffer,
			    int &datasize,UWORD &delay,UWORD speed) = 0;
  //
  // After a written command frame, either sent or test the checksum and flush the
  // contents of the buffer out. For block transfer, SIO does this for us. Otherwise,
  // we must do it manually.
  virtual UBYTE FlushBuffer(const UBYTE *,UWORD &,UWORD &)
  {
    // Default is to rely on the SIO checksumming, send a complete.
    return 'C';
  }
  //
  // Execute a status-only command that does not read or write any data except
  // the data that came over AUX1 and AUX2. This returns the command status of the
  // device.
  virtual UBYTE ReadStatus(const UBYTE *CommandFrame,UWORD &delay,UWORD &speed) = 0;
  //
  // Rather exotic concurrent read/write commands. These methods are used for
  // the 850 interface box to send/receive bytes when bypassing the SIO. In these
  // modes, clocking comes from the external 850 interface, and SIO remains unactive.
  //
  // Check whether we have a concurrent byte to deliver. Return true and the byte
  // if so. Return false otherwise.
  virtual bool ConcurrentRead(UBYTE &)
  {
    // a typical serial device does not have concurrent reading
    return false;
  }
  //
  // Check whether this device is able to accept a concurrently written byte. 
  // Return true if so, and then deliver the byte to the backend of this device.
  // Otherwise, return false.
  virtual bool ConcurrentWrite(UBYTE)
  {
    // The default serial device does not support concurrent mode.
    return false;
  }
  //
  // Check whether this device accepts two-tone coded data (only the tape does)
  // Returns true if the device does, returns false otherwise.
  virtual bool TapeWrite(UBYTE)
  {
    // The default serial device does not.
    return false;
  }
};
///

///
#endif
