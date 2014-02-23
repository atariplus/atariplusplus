/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: tape.cpp,v 1.5 2013/06/04 21:12:43 thor Exp $
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
///

/// Tape::Tape
// Construct a tape drive.
Tape::Tape(class Machine *mach,const char *name)
  : SerialDevice(mach,name,0x60), VBIAction(mach),
    Pokey(NULL), SIO(NULL), TapeImg(NULL), File(NULL),
    Playing(false), Recording(false), ReadNextRecord(false), RecordSize(0),
    NTSC(false), IRGCounter(0), MotorOffCounter(0), EOFGap(3000), TicksPerFrame(0), 
    ImageToLoad(NULL), ImageName(NULL)
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
  
  delete TapeImg;TapeImg = NULL;
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
	} else if (Pokey && RecordSize > 0) {
	  memcpy(OutputBuffer,Buffer,RecordSize);
	  // 1484 is the nominal "baud rate" in pokey hi-res clocks.
	  // +7 is the additional reset delay of the pokey timer.
	  Pokey->SignalSerialBytes(OutputBuffer,RecordSize,(1484 + 7) * 20 / 114 + 1,1484 + 7);
	  // 114 is the number of 1.79MHz ticks per line.
	  IRGCounter     = ((1484 + 7) * 20 * RecordSize + 114 * TicksPerFrame - 1) / (114 * TicksPerFrame);
	  //
	  ReadNextRecord = true;
	}
      } else if (Playing && Recording && TapeImg) {
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
	  TapeImg->WriteChunk(Buffer,RecordSize,IRGSize);
	  RecordSize = 0;
	}
	Playing   = false;
	Recording = false;
	delete TapeImg;TapeImg = NULL;
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
  if (Playing && Recording && TapeImg && SIO->isMotorEnabled()) {
    // If we waited more than two VBIs for more data, create a new record.
    if (IRGCounter > 2) {
      // Write the last one if we have.
      if (RecordSize > 0) {
	TapeImg->WriteChunk(Buffer,RecordSize,IRGSize);
      }
      // Got the first byte of the record. Now measure the IRG size. We counted the number of VBIs.
      int irg = IRGCounter * TicksPerFrame * 1000 / 15700;
      if (irg > 65535)
	irg = 65535;
      IRGSize    = irg;
      RecordSize = 0;
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
  
  if (File) {
    fclose(File);
    File = NULL;
  }

  delete TapeImg;TapeImg = NULL;
}
///

/// Tape::OpenImage
// Open or create a tape image, depending
// on the settings.
void Tape::OpenImage(void)
{
  if (TapeImg == NULL && ImageName && *ImageName) {
    if (Recording) {
      File = fopen(ImageName,"wb");
      if (File == NULL) {
	ThrowIo("Tape::ParseArgs","unable to create a new tape archive");
      } else {
	TapeImg = new class CASFile(File);
	TapeImg->CreateCASHeader();
	IRGCounter = 0;
	RecordSize = 0;
      }
    } else {
      File = fopen(ImageName,"rb");
      // For the time, ignore errors here.
      IRGCounter = 0;
      RecordSize = 0;
      //
      // Create the new tape reader if we have input.
      if (File) {
	TapeImg = new class CASFile(File);
	ReadNextRecord = true;
	//IRGCounter     = (NTSC)?(864):(720);
	IRGCounter     = (NTSC)?(6):(5);
      } else {
	if (ImageToLoad)
	  *ImageToLoad = 0;
	Playing    = false;
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
    { {"PAL" ,false},
      {"NTSC",true },
      {NULL ,0}
    };
  LONG val;

  val = NTSC;

  if (ImageToLoad && *ImageToLoad)
    eject = false;

  args->DefineTitle("Tape");
  args->DefineSelection("VideoMode","sets the video mode for timing the tape",videovector,val);
  args->DefineLong("MotorOffEOFGap","time in ms after which a motor stop will be detected as EOF",20,10000,EOFGap);
  args->DefineFile("Image","sets the CAS file to load into the tape recorder",ImageToLoad,true,true,false);
  args->DefineBool("Play","press the play button on the tape recorder",Playing);
  args->DefineBool("Record","press the record button on the tape recorder",Recording);
  args->DefineBool("Eject","unload the tape from the recorder",eject);

  NTSC = (val)?(true):(false);

  if (NTSC) {
    TicksPerFrame = 262;
  } else {
    TicksPerFrame = 312;
  }
  
  if ((eject || (ImageToLoad && *ImageToLoad && 
		 (ImageName == NULL || ImageToLoad == NULL || strcmp(ImageToLoad,ImageName))))) {
    // Avoid disk changes if possible.
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
      if (File) {
	fclose(File);
	File = NULL;
      }
      delete TapeImg; TapeImg = NULL;
      
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
		   "\tMotor is         : %s\n"
		   "\tPlay is          : %s\n"
		   "\tRecord is        : %s\n",
		   ImageName?ImageName:"",
		   motorstatus,
		   Playing?("pressed"):("released"),
		   Recording?("pressed"):("released")
		   );
}
///

