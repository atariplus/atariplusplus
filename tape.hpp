/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: tape.hpp,v 1.12 2015/12/11 16:27:36 thor Exp $
 **
 ** In this module: Support for the dump tape.
 **********************************************************************************/

#ifndef TAPE_HPP
#define TAPE_HPP

/// Includes
#include "types.hpp"
#include "serialdevice.hpp"
#include "pokey.hpp"
#include "vbiaction.hpp"
///

/// Forewards
class SIO;
class Pokey;
class Monitor;
class ArgParser;
class CASFile;
class TapeImage;
///

/// Tape
// The tape is a specific serial device, and very special in the
// sense that it is especially dumb. It only listens to the data
// from the serial port when the motor is on (SIO arranges this for us)
// but it posts its data to pokey no matter what.
class Tape : public SerialDevice, private VBIAction {
  // 
  // The pokey handle into which we push data directly.
  class Pokey     *Pokey;
  //
  // SIO, for checking the motor line.
  class SIO       *SIO;
  // 
  // Currently inserted tape.
  class TapeImage *TapeImg;
  //
  // Current file to the tape, handled separately.
  FILE            *File;
  //
  // Handling flags. Recording or playing.
  bool             Playing;
  bool             Recording;
  //
  // Write the output as WAV file?
  bool             RecordAsWav;
  //
  // Ready to read the next record?
  bool             ReadNextRecord;
  //
  // Size of the record in bytes.
  UWORD            RecordSize;
  //
  // Size of the IRG in msecs
  UWORD            IRGSize;
  //
  // The timing flag - PAL or NTSC
  bool             NTSC;
  //
  // This flag is set in case the video mode comes from the machine.
  bool             isAuto;
  //
  // Inter-gap counter - measures the delay.
  LONG             IRGCounter;
  //
  // Counts the time period the motor is off.
  // If the motor is off for too long, an EOF is assumed
  // and the tape file is closed.
  LONG             MotorOffCounter;
  //
  // Time period in milliseconds after which an EOF will
  // be assumed when the motor is off.
  LONG             EOFGap;
  //
  // 15Khz ticks per frame.
  LONG             TicksPerFrame;
  //
  // Input buffer or output buffer of the tape.
  UBYTE            Buffer[3+256+1];
  //
  // Output buffer when pokey is ready.
  UBYTE            OutputBuffer[3+256+1];
  //
  // Image the user selected for loading.
  char            *ImageToLoad;
  //
  // Image that is currently loaded.
  char            *ImageName;
  //
  // An indicator if a SIO direct transfer is active.
  bool             SIODirect;
  //
  // Insert a tape to the tape drive.
  void InsertTape();
  //
  // Eject the tape.
  void EjectTape();
  //
  // Open or create a tape image, depending
  // on the settings.
  void OpenImage(void);
  //
  // Fill the record buffer with the next record for reading from tape.
  void FillRecordBuffer(void);
  //
  // Write the last one if we have.
  void FlushRecordBuffer(void);
  //
  // Timing of the tape - identifies gaps and creates them.
  //
  // If pause is set, then the emulator is currently pausing.
  // The "timer" keeps a time stamp updated each VBI and running
  // out as soon as the VBI is over.
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  // Check whether this device accepts the indicated command
  // as valid command, and return the command type of it.
  // As secondary argument, it also returns the number of bytes 
  // in the data frame (if there is any). If this is a write
  // command, datasize can be set to zero to indicate single
  // byte transfer.
  // The tape actually never handles a command frame at all....
  // Except when run thru the SIO patch.
  virtual SIO::CommandType CheckCommandFrame(const UBYTE *commandframe,int &datasize,UWORD);
  // 
  // Acknowledge the command frame. This is called as soon the SIO implementation
  // in the host system tries to receive the acknowledge function from the
  // client. Will return 'A' in case the command frame is accepted. Note that this
  // is only called if CheckCommandFrame indicates already a valid command.
  // The tape does nothing here.
  virtual UBYTE AcknowledgeCommandFrame(const UBYTE *,UWORD &,UWORD &);
  //
  // Read bytes from the device into the system. Returns the command status
  // after the read operation, and installs the number of bytes really written
  // into the data size if it differs from the requested amount of bytes.
  // SIO will call back in case only a part of the buffer has been transmitted.
  // Delay is the number of 15kHz cycles (lines) the command requires for completion.
  virtual UBYTE ReadBuffer(const UBYTE *commandframe,UBYTE *buffer,
			   int &datasize,UWORD &delay,UWORD &speed);
  //  
  // Write the indicated data buffer out to the target device.
  // Return 'C' if this worked fine, 'E' on error. 
  // The tape does nothing of that.
  virtual UBYTE WriteBuffer(const UBYTE *CommandFrame,const UBYTE *buffer,
			    int &datasize,UWORD &delay,UWORD speed);
  //
  // After a written command frame, either sent or test the checksum and flush the
  // contents of the buffer out. For block transfer, SIO does this for us. Otherwise,
  // we must do it manually.
  // The tape does nothing like that.
  virtual UBYTE FlushBuffer(const UBYTE *,UWORD &,UWORD &);
  //
  // Execute a status-only command that does not read or write any data except
  // the data that came over AUX1 and AUX2. This returns the command status of the
  // device. Neither are there any status-related calls for the tape.
  virtual UBYTE ReadStatus(const UBYTE *,UWORD &,UWORD &)
  {
    return 0;
  }
  //
public:
  //
  // Construct the tape.
  Tape(class Machine *mach,const char *name);
  //
  // Destroy it again.
  virtual ~Tape(void);
  //
  // Check whether this device is responsible for the indicated command frame
  // Actually, the tape never feels responsible for a command frame since it
  // is a dumb device. It only reacts on two-tone data comming over a side
  // channel. However, to make the SIO patch work, we need here to react
  // on 0x5f (unit 0, tape).
  virtual bool HandlesFrame(const UBYTE *commandframe);
  //
  // Check whether this device accepts two-tone coded data (only the tape does)
  // Returns true if the device does, returns false otherwise.
  // This is what the tape does. Regardless of whether there is actually anything
  // else also listening to this data.
  virtual bool TapeWrite(UBYTE data);
  //
  // ColdStart and WarmStart
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Argument parser stuff: Parse off command line args
  virtual void ParseArgs(class ArgParser *args);
  //
  // Status display for the monitor.
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Return indicators of whether the tape is playing. If true,
  // the PLAY button is pressed.
  bool isPlaying(void) const
  {
    return Playing;
  }
  //
  // Return indicators of whether the tape is recording. If true,
  // the RECORD button is pressed.
  bool isRecording(void) const
  {
    return Recording;
  }
};
///

///
#endif
