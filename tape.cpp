/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: tape.cpp,v 1.13 2015/12/11 16:27:36 thor Exp $
 **
 ** In this module: Support for the dump tape.
 **********************************************************************************/

/// Includes
#include "tape.hpp"
#include "sio.hpp"
#include "machine.hpp"
#include "pokey.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "casfile.hpp"
#include "tapeimage.hpp"
#include "wavdecoder.hpp"
///

/// Tape::Tape
// Construct a tape drive.
Tape::Tape(class Machine *mach,const char *name)
  : SerialDevice(mach,name,0x60), VBIAction(mach),
    Pokey(NULL), SIO(NULL), TapeImg(NULL), File(NULL),
    Playing(false), Recording(false), RecordAsWav(false), ReadNextRecord(false), RecordSize(0),
    NTSC(false), isAuto(true), IRGCounter(0), MotorOffCounter(0), EOFGap(3000), TicksPerFrame(0), 
    ImageToLoad(NULL), ImageName(NULL), SIODirect(false)
{
  // Actually, even the SIO-ID is just a dummy and not used at all, except
  // Os-internally for administration.
}
///

/// Tape::~Tape
// Delete a tape device
Tape::~Tape(void)
{
  delete TapeImg;
  delete[] ImageToLoad;
  delete[] ImageName;

  if (File)
    fclose(File);
}
/// 

/// Tape::ColdStart
// Turn on the tape.
void Tape::ColdStart(void)
{
  Pokey = machine->Pokey();
  SIO   = machine->SIO(); 
  

  if (NTSC) {
    TicksPerFrame = 262;
  } else {
    TicksPerFrame = 312;
  }

  if (TapeImg) {
    TapeImg->Close();
    delete TapeImg;
    TapeImg = NULL;
  }
  
  Playing   = false;
  Recording = false;

  if (File) {
    fclose(File);
    File = NULL;
  }

  WarmStart();
}
///

/// Tape::WarmStart
// Warm start the tape when the user presses reset.
// Actually, as the tape does not listen to the reset
// signal, nor can it see it, it should do nothing.
// It is just more convenient this way...
void Tape::WarmStart(void)
{
  IRGCounter      = 0;
  RecordSize      = 0;
  MotorOffCounter = 0;
  ReadNextRecord  = false;
  SIODirect       = false;
}
///


/// Tape::CheckCommandFrame
// Check whether this device accepts the indicated command
// as valid command, and return the command type of it.
// As secondary argument, it also returns the number of bytes 
// in the data frame (if there is any). If this is a write
// command, datasize can be set to zero to indicate single
// byte transfer.
// The tape actually never handles a command frame at all....
// Except when run thru the SIO patch.
SIO::CommandType Tape::CheckCommandFrame(const UBYTE *commandframe,int &datasize,UWORD)
{ 
  switch(commandframe[1]) {
  case 'R':
    if (SIO && SIO->isMotorEnabled() && TapeImg && Playing && Recording == false && RecordSize > 4) {
      datasize = RecordSize - 1; // Checksum is not included
      return SIO::ReadCommand;
    }
    break;
  case 'W':
  case 'P':
    if (SIO && SIO->isMotorEnabled() && Playing && Recording) {
      // The sync markers, the record type and the check sum are not part of the
      // deal.
      datasize = 128 + 3; // Plus sync bytes plus record type
      return SIO::WriteCommand;
    }
    break;
  }
  return SIO::InvalidCommand;
}
///

/// Tape::AcknowledgeCommandFrame
// Acknowledge the command frame. This is called as soon the SIO implementation
// in the host system tries to receive the acknowledge function from the
// client. Will return 'A' in case the command frame is accepted. Note that this
// is only called if CheckCommandFrame indicates already a valid command.
// The tape only implements this as part of the SIO patch.
UBYTE Tape::AcknowledgeCommandFrame(const UBYTE *,UWORD &,UWORD &)
{
  //
  // Play with the SIO protocol a bit.
  if (SIO && SIO->isMotorEnabled() && Playing) {
    SIODirect = true;
    return 'A';
  }
  return 'N';
}
///

/// Tape::FillRecordBuffer
// Fill the record buffer with the next record for reading from tape.
void Tape::FillRecordBuffer(void)
{
  UWORD irg;
  RecordSize     = TapeImg->ReadChunk(Buffer,sizeof(Buffer),irg);
  ReadNextRecord = false;
  //
  // Got any data or EOF? On EOF, just stop delivering anything...
  if (RecordSize > 0) {
    // Compute the size in frames until the data starts.
    // irg / 1000 is the time in seconds.
    // 15700 / ticksperframe is the frame rate in Hz.
    IRGCounter = (irg * 15700) / (TicksPerFrame * 1000) + 1;
  }
}
///

/// Tape::FlushRecordBuffer
// Write the last one if we have.
void Tape::FlushRecordBuffer(void)
{
  if (RecordSize > 0) {
    if (TapeImg == NULL && ImageName && *ImageName) {
      File = fopen(ImageName,"wb");
      if (File == NULL) {
	ThrowIo("Tape::FlushRecordBuffer","unable to create a new tape archive");
      } else {
	if (RecordAsWav) {
	  TapeImg = new class WavDecoder(machine,File);
	} else {
	  TapeImg = new class CASFile(File);
	}
	TapeImg->OpenForWriting();
      }
    }
    if (TapeImg)
      TapeImg->WriteChunk(Buffer,RecordSize,IRGSize);
  }
  // Got the first byte of the record. Now measure the IRG size. We counted the number of VBIs.
  int irg = IRGCounter * TicksPerFrame * 1000 / 15700;
  if (irg > 65535)
    irg = 65535;
  IRGSize    = irg;
  RecordSize = 0;
}
///

/// Tape::ReadBuffer
// Read bytes from the device into the system. Returns the command status
// after the read operation, and installs the number of bytes really written
// into the data size if it differs from the requested amount of bytes.
// SIO will call back in case only a part of the buffer has been transmitted.
// Delay is the number of 15kHz cycles (lines) the command requires for completion.
UBYTE Tape::ReadBuffer(const UBYTE *,UBYTE *buffer,int &datasize,UWORD &,UWORD &)
{
  if (Playing && Recording == false && TapeImg && SIO && SIO->isMotorEnabled()) {
    // Check whether we need to update the buffer. If so, do that
    // now.
    if (ReadNextRecord)
      FillRecordBuffer();
    //
    // Check whether we have anything to read. If so, read now.
    if (RecordSize > 1) {
      if (datasize > RecordSize - 1)
	datasize = RecordSize - 1;
      memcpy(buffer,Buffer,datasize);
      // Update the IRG counter, but let's hope the SIO patch picks this
      // up earlier.
      // 114 is the number of 1.79MHz ticks per line.
      IRGCounter     = ((1484 + 7) * 20 * RecordSize + 114 * TicksPerFrame - 1) / (114 * TicksPerFrame);
      ReadNextRecord = true;
    } else {
      // Nothing to read, we're at EOF.
      datasize = 0;
    }
    return 'C';
  }
  return 'N';
}
///  

/// Tape::WriteBuffer
// Write the indicated data buffer out to the target device.
// Return 'C' if this worked fine, 'E' on error. 
// The tape does nothing of that.
UBYTE Tape::WriteBuffer(const UBYTE *cmdframe,const UBYTE *buffer,int &datasize,UWORD &,UWORD)
{ 
  if (Playing && Recording && SIO && SIO->isMotorEnabled()) {
    if (RecordSize > 0) {
      // Now add to this the record/mode specific IRG size.
      FlushRecordBuffer();
    } else {
      IRGSize = UWORD(IRGCounter * TicksPerFrame * 1000 / 15700);
    }
    //
    // Copy the record buffer over.
    if (datasize != 128 + 3) {
      Throw(OutOfRange,"Tape::TapeWrite","Tape buffer size invalid, supports only 132 bytes per record");
    }
    // Copy the data over, also include the checksum.
    memcpy(Buffer,buffer,datasize);
    Buffer[datasize] = SIO->ChkSum(buffer,datasize);
    //
    RecordSize = datasize + 1;
    //
    // Compute the size of the IRG and include it in the next
    // record.
    if (cmdframe[3] & 0x80) {
      IRGSize += 160; // Short IRG;
    } else {
      IRGSize += 2000; // Long IRG;
    }
    IRGCounter = 0;
    return 'C';
  }
  return 'E';
}
///

/// Tape::FlushBuffer
// After a written command frame, either sent or test the checksum and flush the
// contents of the buffer out. For block transfer, SIO does this for us. Otherwise,
// we must do it manually.
// The tape does nothing like that.
UBYTE Tape::FlushBuffer(const UBYTE *,UWORD &,UWORD &)
{
  if (Playing && Recording && SIO && SIO->isMotorEnabled()) {
    return 'A';
  }
  return 'E';
}
///

/// Tape::HandlesFrame
// Check whether this device is responsible for the indicated command frame
// Actually, the tape never feels responsible for a command frame since it
// is a dumb device. It only reacts on two-tone data comming over a side
// channel. However, to make the SIO patch work, we need here to react
// on 0x5f (unit 0, tape).
bool Tape::HandlesFrame(const UBYTE *commandframe)
{
  if (SIO && SIO->isMotorEnabled()) {
    if (commandframe[0] == 0x5f || commandframe[0] == 0x60) {
      return true;
    }
  }
  return false;
}
///

/// Tape::VBI
// Timing of the tape. This triggers the tape data sending.
// If pause is set, then the emulator is currently pausing.
// The "timer" keeps a time stamp updated each VBI and running
// out as soon as the VBI is over.
void Tape::VBI(class Timer *,bool,bool pause)
{
  if (pause == false && SIO) {
    if (SIO->isMotorEnabled()) {
      MotorOffCounter = 0;
      if (Playing && Recording == false) {
	if (IRGCounter > 0) {
	  IRGCounter--;
	} else if (TapeImg && ReadNextRecord) {
	  FillRecordBuffer();
	} else if (Pokey && !SIODirect && RecordSize > 0) {
	  memcpy(OutputBuffer,Buffer,RecordSize);
	  // 1484 is the nominal "baud rate" in pokey hi-res clocks.
	  // +7 is the additional reset delay of the pokey timer.
	  Pokey->SignalSerialBytes(OutputBuffer,RecordSize,(1484 + 7) * 20 / 114 + 1,1484 + 7);
	  // 114 is the number of 1.79MHz ticks per line.
	  IRGCounter     = ((1484 + 7) * 20 * RecordSize + 114 * TicksPerFrame - 1) / (114 * TicksPerFrame);
	  //
	  ReadNextRecord = true;
	}
      } else if (Playing && Recording) {
	// Just measure the time if the record is written.
	IRGCounter++;
      }
    } else if (Playing && RecordSize > 0) {
      //
      // Also count here.
      MotorOffCounter++;
      //
      // If the motor stays off for too long, just consider that this is the end of the file.
      if (MotorOffCounter > EOFGap * 15700 / (TicksPerFrame * 1000)) {
	if (Recording && TapeImg) {
	  // Write the final record if there is one.
	  FlushRecordBuffer();
	}
	Playing   = false;
	Recording = false;
	SIODirect = false;
	if (TapeImg) {
	  TapeImg->Close();
	  delete TapeImg;
	  TapeImg = NULL;
	}
	if (File) {
	  fclose(File);
	  File = NULL;
	}
	MotorOffCounter = 0;
      }
    }
  }
}
///

/// Tape::TapeWrite
// Check whether this device accepts two-tone coded data (only the tape does)
// Returns true if the device does, returns false otherwise.
// This is what the tape does. Regardless of whether there is actually anything
// else also listening to this data.
bool Tape::TapeWrite(UBYTE data)
{
  if (Playing && Recording && SIO->isMotorEnabled()) {
    // If we waited more than two VBIs for more data, create a new record.
    if (IRGCounter > 2) {
      // Write the last one if we have.
      FlushRecordBuffer();
    }
    //
    if (RecordSize >= sizeof(Buffer)) {
      Throw(OutOfRange,"Tape::TapeWrite","Tape buffer overrun, supports at most 132 bytes per record");
    } else {
      IRGCounter = 0;
      Buffer[RecordSize++] = data;
      return true;
    }
  }
  return false;
}
///

/// Tape::InsertTape
// Insert a tape to the tape drive.
void Tape::InsertTape()
{
  bool oldplay   = Playing;
  bool oldrecord = Recording;
  EjectTape();

  if (ImageToLoad && *ImageToLoad) {
    delete[] ImageName; ImageName = NULL;
    
    ImageName      = new char[strlen(ImageToLoad) + 1];
    strcpy(ImageName,ImageToLoad);
    
    Playing        = oldplay;
    Recording      = oldrecord;
  }
}
///

/// Tape::EjectTape
// Eject the tape.
void Tape::EjectTape()
{
  Playing         = false;
  Recording       = false;
  IRGCounter      = 0;
  RecordSize      = 0;
  MotorOffCounter = 0;

  if (TapeImg) {
    TapeImg->Close();
    delete TapeImg;
    TapeImg = NULL;
  }
  
  if (File) {
    fclose(File);
    File = NULL;
  }
}
///

/// Tape::OpenImage
// Open or create a tape image, depending
// on the settings.
void Tape::OpenImage(void)
{
  if (TapeImg == NULL && ImageName && *ImageName) {
    if (Recording) {
      IRGCounter = 0;
      RecordSize = 0;
    } else {
      File = fopen(ImageName,"rb");
      // For the time, ignore errors here.
      IRGCounter = 0;
      RecordSize = 0;
      //
      // Create the new tape reader if we have input.
      if (File) {
	TapeImg        = TapeImage::CreateImageForFile(machine,File);
	TapeImg->OpenForReading();
	ReadNextRecord = true;
	//IRGCounter     = (NTSC)?(864):(720);
	IRGCounter     = (NTSC)?(6):(5);
      } else {
	if (ImageToLoad)
	  *ImageToLoad = 0;
	Playing    = false;
	ThrowIo("Tape::OpenImage","unable to open the tape file");
      }
    }
  }
}
///

/// Tape::ParseArgs
// Argument parser stuff: Parse off command line args
void Tape::ParseArgs(class ArgParser *args)
{
  bool eject     = true;
  bool oldplay   = Playing;
  bool oldrecord = Recording;
  static const struct ArgParser::SelectionVector videovector[] =
    { {"Auto",2    },
      {"PAL" ,0    },
      {"NTSC",1    },
      {NULL ,0}
    };
  LONG val;

  val = (NTSC)?(1):(0);
  if (isAuto)
    val = 2;

  if (ImageToLoad && *ImageToLoad)
    eject = false;

  args->DefineTitle("Tape");
  args->DefineSelection("TapeTimeBase","sets the timing basis for the tape",videovector,val);
  args->DefineLong("MotorOffEOFGap","time in ms after which a motor stop will be detected as EOF",20,10000,EOFGap);
  args->DefineFile("Image","sets the CAS file to load into the tape recorder",ImageToLoad,true,true,false);
  args->DefineBool("Play","press the play button on the tape recorder",Playing);
  args->DefineBool("Record","press the record button on the tape recorder",Recording);
  args->DefineBool("Eject","unload the tape from the recorder",eject);
  args->DefineBool("RecordAsWav","write tape output as WAV file",RecordAsWav);

  switch(val) {
  case 0:
    NTSC   = false;
    isAuto = false;
    break;
  case 1:
    NTSC   = true;
    isAuto = false;
    break;
  case 2:
    NTSC   = machine->isNTSC();
    isAuto = true;
    break;
  }
  
  if (NTSC) {
    TicksPerFrame = 262;
  } else {
    TicksPerFrame = 312;
  }
  
  if ((eject || (ImageToLoad && *ImageToLoad && 
		 (ImageName == NULL || ImageToLoad == NULL || strcmp(ImageToLoad,ImageName))))) {
    // Avoid tape changes if possible.
    if (eject && ImageName) {
      EjectTape();
    } else {
      if (ImageName == NULL || ImageToLoad == NULL || strcmp(ImageToLoad,ImageName)) {
        InsertTape();
	OpenImage();
      }
    }
  } else {
    // Image did not change.
    if (Playing && (oldplay == false || Recording != oldrecord) && ImageName && *ImageName) {
      // Ok, a tape is ready. Get it.
      if (TapeImg) {
	TapeImg->Close();
	delete TapeImg;
	TapeImg = NULL;
      }
      if (File) {
	fclose(File);
	File = NULL;
      }
      OpenImage();
    }
  }
}
///

/// Tape::DisplayStatus
// Status display for the monitor.
void Tape::DisplayStatus(class Monitor *mon)
{
  const char *motorstatus;

  if (SIO && SIO->isMotorEnabled()) {
    motorstatus = "running";
  } else {
    motorstatus = "stopped";
  }
  
  mon->PrintStatus("\tImage file       : %s\n"
		   "\tIRG Counter      : %ld\n"
		   "\tMotor is         : %s\n"
		   "\tPlay is          : %s\n"
		   "\tRecord is        : %s\n",
		   ImageName?ImageName:"",
		   long(IRGCounter),
		   motorstatus,
		   Playing?("pressed"):("released"),
		   Recording?("pressed"):("released")
		   );
}
///

