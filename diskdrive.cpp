/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: diskdrive.cpp,v 1.73 2011-12-28 00:07:47 thor Exp $
 **
 ** In this module: Support for the serial (external) disk drive.
 **********************************************************************************/

/// Includes
#include "diskdrive.hpp"
#include "serialdevice.hpp"
#include "sio.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "string.hpp"
#include "imagestream.hpp"
#include "filestream.hpp"
#include "zstream.hpp"
#include "diskimage.hpp"
#include "xfdimage.hpp"
#include "atrimage.hpp"
#include "binaryimage.hpp"
#include "streamimage.hpp"
#include "dcmimage.hpp"
#include "casstream.hpp"
#include "stdio.hpp"
#include "new.hpp"
#include <errno.h>
///

/// Possible Disk layouts
static const struct DriveLayout {
  ULONG heads;
  ULONG tracks;
  ULONG secspertrack;
  ULONG secsize;
  //
  bool operator==(const struct DriveLayout &o) const
  {
    return ((o.heads        == heads) &&
	    (o.tracks       == tracks) &&
	    (o.secspertrack == secspertrack) &&
	    (o.secsize      == secsize));
  }
} drive_layouts[] = {
  {1,40,18,128}, // single sided, single density, 40 tracks
  {1,40,26,128}, // single sided, enhanced      , 40 tracks
  {1,40,18,256}, // single sided, double density, 40 tracks
  {1,40, 9,512}, // single sided, double density, 40 tracks
  {1,40,18,512}, // single sided, high   density, 40 tracks
  
  {2,40,18,128}, // double sided, single density, 40 tracks
  {2,40,26,128}, // double sided, enhanced      , 40 tracks
  {2,40,18,256}, // double sided, double density, 40 tracks
  {2,40, 9,512}, // double sided, double density, 40 tracks
  {2,40,18,512}, // double sided, high   density, 40 tracks

  {1,80,18,128}, // single sided, single density, 80 tracks
  {1,80,26,128}, // single sided, enhanced      , 80 tracks
  {1,80,18,256}, // single sided, double density, 80 tracks
  {1,80, 9,512}, // single sided, double density, 80 tracks
  {1,80,18,512}, // single sided, high   density, 80 tracks
  
  {2,80,18,128}, // double sided, single density, 80 tracks
  {2,80,26,128}, // double sided, enhanced      , 80 tracks
  {2,80,18,256}, // double sided, double density, 80 tracks
  {2,80, 9,512}, // double sided, double density, 80 tracks
  {2,80,18,512}, // double sided, high   density, 80 tracks
  
  {1,35 ,26,128}, // 8 inch drive, single sided, single density, 35 tracks
  {1,77 ,26,128}, // 8 inch drive, single sided, single density, 77 tracks
  {1,35 ,26,256}, // 8 inch drive, single sided, double density, 35 tracks
  {1,77 ,26,256}, // 8 inch drive, single sided, double density, 77 tracks
  
  {2,35 ,26,128}, // 8 inch drive, double sided, single density, 35 tracks
  {2,77 ,26,128}, // 8 inch drive, double sided, single density, 77 tracks
  {2,35 ,26,256}, // 8 inch drive, double sided, double density, 35 tracks
  {2,77 ,26,256}, // 8 inch drive, double sided, double density, 77 tracks

  {0,0  ,0 ,0  }
};
///

/// DiskDrive::DiskDrive
DiskDrive::DiskDrive(class Machine *mach,const char *name,int id)
  : SerialDevice(mach,name,'1'+id), DriveId(id),
    Bank0000(NULL), Bank8000(NULL),
    BankC000(NULL), BankE000(NULL)
{
  DiskStatus       = None;
  // Default is to turn only drive #1 on
  ProtectionStatus = (id == 0)?(UnLoaded):(Off);
  ImageType        = Unknown;
  ImageStream      = NULL;
  Disk              = NULL;
  ImageName        = NULL;
  ImageToLoad      = NULL;
  DisplayControl   = 0;
  SpeedControl     = 9; // speedy default speed, whatever this means...
  //
  // Install drive default settings: 128 byte sectors, 720 of them.
  SectorSize       = 128;
  SectorCount      = 720;
  SectorsPerTrack  = 18;
  //
  // Provide default configuration.
  Emulates815      = true;
  EmulatesSpeedy   = true;
  RunningTest      = 0xff; // none
  LastSector       = 1;
}
///

/// DiskDrive::~DiskDrive
DiskDrive::~DiskDrive(void)
{
  delete ImageStream;
  delete Disk;
  delete[] ImageName;
  delete[] ImageToLoad;
  delete[] Bank0000;
  delete[] Bank8000;
  delete[] BankC000;
  delete[] BankE000;
}
///

/// DiskDrive::ColdStart
void DiskDrive::ColdStart(void)
{
}
///

/// DiskDrive::WarmStart
void DiskDrive::WarmStart(void)
{
  // As the disk drive is not connected to the
  // CPU RESET line, nothing can happen here.
}
///

/// DiskDrive::InstallUserCommand
// For Happy's only: Install a user definable command
// FIXME: This is not yet implemented as it would require
// a detailed knowledge of the Happy hardware and a CPU
// emulation on top. The latter, at least, would be easy.
UBYTE DiskDrive::InstallUserCommand(const UBYTE *buffer,int)
{
  struct FloppyCmd *fcmd = UserCommands;
  int i;
  bool found = false;
  
  //  
  // Check whether the command to be defined is already defined
  for (i=0;i<MaxUserCommands;i++,fcmd++) {
    if (fcmd->CmdChar == buffer[0]) {
      found = true;
      break;
    }
  }

  // 
  if (found) {
    if (buffer[2] | buffer[3]) {
      // Define the user function now
      fcmd->Procedure = buffer[2] | (buffer[3] << 8);
      return 'C'; // worked
    } else {
      // erase it otherwise
      fcmd->Procedure = 0;
      fcmd->CmdChar   = 0;
      return 'C';
    }
  }

  // Otherwise, seek for a free slot and insert it
  if (buffer[2] | buffer[3]) {
    fcmd = UserCommands;
    for(i=0;i<MaxUserCommands;i++,fcmd++) {
      if (fcmd->Procedure == 0) {
	// Ok, use this slot and let it go.
	fcmd->CmdChar   = buffer[0];
	fcmd->Procedure = buffer[2] | (buffer[3] << 8);
	return 'C';
      }
    }
    return 'E'; // did not...
  }
  
  return 'C';
}
///

/// DiskDrive::JumpStatusType
// For Happy only: Jump to a Happy subroutine returning a status
// and identify the command type ("the serial frame") it uses.
SIO::CommandType DiskDrive::JumpStatusType(ADR address,int &datasize)
{
  switch (address) {
  case 0xff0f:   // This is probably a plain drive reset
    break;
  case 0xffa5:   // No idea what this is
    break;
  case 0xffb4:   // This is probably a RAM test.
    datasize = 2;
    return SIO::ReadCommand;
  case 0xffba:   // This is probably a motor test. Should return 288 RPM
    datasize = 2;
    return SIO::ReadCommand;
  case 0xffb7:   // This is probably a ROM test.
    break;
  case 0x9e00:   // This sets the drive display. We ignore this
    datasize = 3;
    return SIO::WriteCommand;
  default:
    machine->PutWarning("Unknown floppy entry point %4x.\n",address);
  }
  return SIO::StatusCommand;
}
///

/// DiskDrive::JumpStatus
// For Happy only: Jump to a Happy subroutine returning a status.
UBYTE DiskDrive::JumpStatus(ADR address,UBYTE *buffer)
{
  switch (address) {
  case 0xff0f:   // This is probably a plain drive reset
    return 'C';
  case 0xffa5:   // No idea what this is
    return 'C';
  case 0xffb4:   // This is probably a RAM test.
    buffer[0] = 0;
    buffer[1] = 0;
    return 'C';
  case 0xffba:   // This is probably a motor test. Should return 288 RPM
    buffer[0] = 40;
    buffer[1] = 1;
    return 'C';
  case 0xffb7:   // This is probably a ROM test.
    return 'C';
  case 0x9e00:   // This sets the drive display. We ignore this
    return 'C';
  default:
    machine->PutWarning("Unknown floppy entry point %4x.\n",address);
  }
  return 0;
}
///

/// DiskDrive::SpeedyBankSize
// Get the size of a speedy RAM/ROM bank and return its size.
// return zero if the sector corresponds to a true hardware 
// floppy sector on disk, or if we don't know.
int DiskDrive::SpeedyBankSize(ADR sector)
{
  if (EmulatesSpeedy) {
    if (sector == 0) {
      return 128; // Zero page
    } else if (sector>=0x8000 && sector<=0x9fff && ((sector & 0xff) == 0)) {
      return 256;
    } else if (sector>=0xc000 && sector<=0xc0ff && ((sector & 0xff) == 0)) {
      return 256;
    } else if (sector>=0xe000 && sector<=0xffff && ((sector & 0x7f) == 0)) {
      return 128;
    } 
  }
  return 0;
}
///

/// DiskDrive::SpeedyBank
// Return a pointer to the speedy bank in question.
UBYTE *DiskDrive::SpeedyBank(ADR sector)
{
  if (sector == 0) {
    if (Bank0000 == NULL) {
      // Ok, rebuild bank zero and clear it.
      Bank0000 = new UBYTE[0x080];
      memset(Bank0000,0,0x80);
    }
    return Bank0000;
  } else if (sector>=0x8000 && sector<=0x9fff && ((sector & 0xff) == 0)) {
    if (Bank8000 == NULL) {
      Bank8000 = new UBYTE[0x2000];
      memset(Bank8000,0,0x2000);
    }
    return Bank8000 + (sector - 0x8000);
  } else if (sector>=0xc000 && sector<=0xc0ff && ((sector & 0xff) == 0)) {
    if (BankC000 == NULL) {
      BankC000 = new UBYTE[0x0100];
      memset(BankC000,0,0x0100);
    }
    return BankC000 + (sector - 0xc000);
  } else if (sector>=0xe000 && sector<=0xffff && ((sector & 0x7f) == 0)) {
    if (BankE000 == NULL) {
      BankE000 = new UBYTE[0x2000];
      memset(BankE000,0,0x2000);
    }
    return BankE000 + (sector - 0xe000);
  } 
  return NULL;
}
///

/// DiskDrive::WriteStatusBlock
// Define some disk drive geometry data here
// This is only for enhanced/extended drives
UBYTE DiskDrive::WriteStatusBlock(const UBYTE *buffer,int)
{
  const struct DriveLayout *layout = drive_layouts;
  struct DriveLayout req;
  //
  // Copy the request over.
  req.heads        = buffer[4] + 1;
  req.tracks       = buffer[0];
  req.secspertrack = UWORD((buffer[2] << 8) | buffer[3]);
  req.secsize      = UWORD((buffer[6] << 8) | buffer[7]);

  while(layout->heads) {
    // Check whether we found a match.
    if (*layout == req) {
      // Found a match.
      SectorSize      = UWORD(req.secsize);
      SectorsPerTrack = req.secspertrack;
      SectorCount     = req.heads * req.tracks * req.secspertrack;
      return 'C';
    }
    layout++;
  }
  return 'E';
}
///

/// DiskDrive::ReadStatusBlock
// Read the disk geometry and fill it into the buffer
// Only for extended drives
UBYTE DiskDrive::ReadStatusBlock(UBYTE *buffer)
{
  const struct DriveLayout *layout = drive_layouts;
  int heads;
  int tracks;
  int secpertrack;
  int ctl = 0;

  // Check whether a match can be found.
  while(layout->heads) {
    ULONG seccount = layout->heads * layout->tracks * layout->secspertrack;
    if (layout->secsize      == SectorSize      && 
	layout->secspertrack == SectorsPerTrack && 
	SectorCount          == seccount)
      break; // found a match
    layout++;
  }

  // If no valid combination could be found, make this a "hard drive partition"
  if (layout->heads == 0) {
    tracks      = 1;
    heads       = 1;
    secpertrack = SectorCount;
    if (secpertrack > 0x100ffff)
      secpertrack = 0x100ffff;
    if (secpertrack > 0xffff) {
      ctl      |= 0x08; // large drive;
      heads     = secpertrack >> 16;
      // Only the low 16 bits will go into the drive layout
    }
  } else {
    // Valid layout.
    tracks      = layout->tracks;
    heads       = layout->heads;
    secpertrack = layout->secspertrack;	
    if (tracks == 35 || tracks == 77)
      ctl |= 0x02;	// 8 inch drive
  }

  if (SectorSize > 128) {
    ctl |= 0x04;	// MFM
  }

  buffer[0]  = tracks;
  buffer[1]  = 1;          // step rate. No idea what this means
  buffer[2]  = UBYTE(secpertrack>>8);
  buffer[3]  = UBYTE(secpertrack);  // total number of sectors per track
  buffer[4]  = heads - 1;	    // number of heads minus one.
  buffer[5]  = ctl;
  buffer[6]  = UBYTE(SectorSize >> 8);
  buffer[7]  = UBYTE(SectorSize); // size of a sector in bytes
  buffer[8]  = 255;       // drive is online
  buffer[9]  = 0;         // transfer speed. Whatever this means
  buffer[10] = 0;         // reserved
  buffer[11] = 0;         // reserved
  
  return 'C';             // is 12 bytes long
}
///

/// DiskDrive::DriveStatus
// Return the drive status bytes (four bytes)
//
// Status Request from Atari 400/800 Technical Reference Notes
//
// DVSTAT + 0	Command Status
// DVSTAT + 1	Hardware Status
// DVSTAT + 2	Timeout
// DVSTAT + 3	Unused
//
// Command Status Bits
//
// Bit 0 = 1 indicates an invalid command frame was received
// Bit 1 = 1 indicates an invalid data frame was received
// Bit 2 = 1 indicates that a PUT operation was unsuccessful
// Bit 3 = 1 indicates that the disk is write protected
// Bit 4 = 1 indicates active/standby
// Bit 5 = 1 indicates double density
// Bit 7 = 1 indicates enhanced density disk (1050 format)
//
UBYTE DiskDrive::DriveStatus(UBYTE *buffer)
{
  buffer[0] = 0;
  buffer[1] = 0;

  if (ProtectionStatus != UnLoaded) {
    buffer[0] |= 16;  // drive is active (so we say)
    buffer[1] |= 128; // drive has been loaded
  }

  if (ProtectionStatus == ReadOnly) {
    buffer[0] |= 8;   // drive is read-only
    buffer[1] |= 64;
  }

  switch(DiskStatus) {
  case Single:
    if (LastSector <= SectorsPerTrack)
      buffer[1] |= 4; // indicate the track #0 flag
    break;
  case Enhanced:
    if (LastSector <= SectorsPerTrack)
      buffer[1] |= 4; // indicate the track #0 flag
    buffer[0]   |= 128;
    break;
  case Double:
  case High:
    if (LastSector <= SectorsPerTrack)
      buffer[1] |= 4; // indicate the track #0 flag
    buffer[0]   |= 32;
    break;
  default: // Shut up the compiler
    break;
  }
  
  buffer[2] = 7; // drive timeout in seconds
  buffer[3] = 0;

  return 'C';
}
///

/// DiskDrive::FormatSingle
// Emulate formatting a disk in single density
// (or 720 sectors if you'd say so)
// Fill the status bytes into the buffer and return the
// disk status
UBYTE DiskDrive::FormatSingle(UBYTE *buffer,UWORD aux)
{
  if (ProtectionStatus == ReadWrite) {
    if (aux == 0x411) {
      SectorCount = 1040; // This is in fact an enhanced density write
      SectorSize  = 128;
    } else {
      // Keep the geometry as defined by the set density command (KMK)
      //SectorSize  = (DiskStatus == Double)?256:128;
      //SectorCount = 720;
    }  
    //
    // Do not format yet if we are just asking about the
    // size of the returned information.
    if (CreateNewImage(SectorSize,SectorCount) == 'C') {
      // Return the sector-ok flags
      memset(buffer,0x00,SectorSize);
      buffer[0] = buffer[1] = 0xff; // 0xffff = end of list
      return 'C';
    }
  }
  // Return an error in case there is no disk here.
  return 'E';
}
///

/// DiskDrive::FormatEnhanced
// Emulate formatting a disk in enhanced density
// (or 1040 sectors if you'd say so)
// Fill the status bytes into the buffer and return the
// size of the generated status.
UBYTE DiskDrive::FormatEnhanced(UBYTE *buffer,UWORD)
{
  return FormatSingle(buffer,0x411);
}
///

/// DiskDrive::CreateNewImage
// Create a new file image for writing/formatting and return the SIO
// status of it.
UBYTE DiskDrive::CreateNewImage(UWORD sectorsize,ULONG sectorcount)
{ 
  // Unload the current disk since we need to rebuild a new one, but
  // under the same name. We therefore copy the name into the backup
  // and then load the image under that name.
  if (ImageToLoad == NULL || strcmp(ImageToLoad,ImageName)) {
    delete[] ImageToLoad;
    ImageToLoad = NULL;
    ImageToLoad = new char[strlen(ImageName) + 1];
    strcpy(ImageToLoad,ImageName);
  }
  EjectDisk();
  //
  // Create a new image stream and format it here.
  try {
    ImageStream = new class FileStream;
    if (ImageStream->FormatImage(ImageToLoad) == false) {
      EjectDisk();
      return 'E';
    }
    // Ok, that worked. Now format an ATR image on top of it.
    ATRImage::FormatDisk(ImageStream,sectorsize,sectorcount);
    // That worked, either. Now re-open the image from that name. For
    // that, first clean up.
    delete ImageStream;
    ImageStream = NULL;
    // Do not write protect since the disk was obviously not write protected
    // when we began formatting.
    InsertDisk(false);
    return 'C';
  } catch(...) {
    // In case of an error somewhere in here, we do not cause an error to the
    // user, but rather forward this error back to SIO.
    EjectDisk();
    return 'E';
  }
}
///

/// DiskDrive::LoadImage
// Load the disk drive with a new image of the given name, do not
// keep that name. This is the low-level disk-insert method.
// Returns a boolean success indicator
void DiskDrive::LoadImage(const char *filename)
{
  UBYTE buffer[4];
  //
  // Cleanup the previous stream by ejecting it.
  EjectDisk();
  //
  // Re-open the stream to the given file name.
  ImageStream = new class FileStream;
  //
  // If the image does not exist, create a new blank disk image.
  try {
    ImageStream->OpenImage(filename);
  } catch(const AtariException &ex) {
    if (ex.TypeOf() != AtariException::Ex_IoErr)
      throw ex;
    //    
    if (ImageStream->FormatImage(filename) == false) {
      throw ex;
    }
    // Ok, that worked. Now format an ATR image on top of it,
    // use default parameters that can be used by all drives.
    ATRImage::FormatDisk(ImageStream,128,720);
    //
    // Now re-open.
    delete ImageStream;ImageStream = NULL;
    //
    ImageStream = new class FileStream;
    //
    // Try again to open it.
    ImageStream->OpenImage(filename);
  }
  //
  // Read the first two bytes to identify the type of the image.
  if (ImageStream->Read(0,buffer,4)) {
    // Check for the type of the file here now.
    if (buffer[0] == 0x1f && buffer[1] == 0x8b) {
      // This is a gzip'd file. Check whether we have the zlib
#if HAVE_ZLIB_H && HAVE_GZOPEN
      delete ImageStream;
      // Open an image by the ZLib
      ImageStream = NULL;
      ImageStream = new class ZStream;
      ImageStream->OpenImage(filename);
      OpenDiskFromStream();
#else
      // Otherwise, we cannot open it.
      Throw(NotImplemented,"DiskDrive::LoadImage",".gz files are not supported by compile time options");
#endif
    } else if (buffer[0] == 0x50 && buffer[1] == 0x4b) {
      // This is a PKZip'd file. 
      //
      // FIXME: Need to open it.
      Throw(NotImplemented,"DiskDrive::LoadImage",".zip files are not yet supported");
    } else if (buffer[0] == 'F' && buffer[1] == 'U' && buffer[2] == 'J' && buffer[3] == 'I') {
      // This is a CAS archive stream.
      delete ImageStream;
      ImageStream = NULL;
      ImageStream = new class CASStream;
      ImageStream->OpenImage(filename);
      OpenDiskFromStream();
    } else {
      // Now build the stream from it.
      OpenDiskFromStream();
    }
  } else {
    if (errno == 0)
      Throw(InvalidParameter,"DiskDrive::LoadImage","invalid disk image");
    else
      ThrowIo("DiskDrive::LoadImage","unable to read the header of the disk image");
  }
}
///

/// DiskDrive::OpenDiskFromStream
// Open/Create a disk from a given ImageStream,
// checking its type and building the proper type of
// disk from it.
void DiskDrive::OpenDiskFromStream(void)
{  
  UBYTE buffer[2];
#if CHECK_LEVEL > 0
  // Check whether the disk is currently unloaded.
  if (Disk)
    Throw(ObjectExists,"DiskDrive::OpenDiskFromStream","the old image is still loaded, eject it first");
#endif
  //
  // Read the first two bytes to identify the type of the image.
  errno = 0;
  if (ImageStream->Read(0,buffer,2)) {
    // Check for the type of the file here now.
    if (buffer[0] == 0x96 && buffer[1] == 0x02) {
      // This is an ATR image. Open it from here.
      Disk = new class ATRImage(machine);
      Disk->OpenImage(ImageStream);
      ImageType   = ATR;
    } else if (buffer[0] == 0xff && buffer[1] == 0xff) {
      // This is a binary load file.
      Disk = new class BinaryImage(machine);
      Disk->OpenImage(ImageStream);
      ImageType   = CMD;
    } else if (buffer[0] == 0x00 && buffer[1] == 0x00) {
      // This is a basic file.
      Disk = new class StreamImage(machine,"PROGRAM.BAS");
      Disk->OpenImage(ImageStream);
      ImageType   = FILE;
    } else if (buffer[0] == 0xfe && buffer[1] == 0xfe) {
      // This is a MAC.65 file.
      Disk = new class StreamImage(machine,"PROGRAM.ASM");
      Disk->OpenImage(ImageStream);
      ImageType   = FILE;
    } else if (buffer[0] == 0xfa) {
      // This is a DCM image
      Disk = new class DCMImage(machine);
      Disk->OpenImage(ImageStream);
      ImageType   = DCM;
    } else if (buffer[0] == 0xf9) {
      // This is a multi-volume DCM stream. We don't support it, though.
      Throw(NotImplemented,"DiskDrive::OpenDiskFromStream","multi-volume DCM images are not yet supported");
    } else {
      // The fallback option: An .xfd stream.
      Disk = new class XFDImage(machine);
      Disk->OpenImage(ImageStream);
      ImageType   = XFD;
    }
    // Check whether this thingy is write-protected.
    ProtectionStatus = Disk->ProtectionStatus()?(ReadOnly):(ReadWrite);
    //    
    // Fill in sector size and sector count since we need it later
    // for the drive status anyhow.
    {
      const struct DriveLayout *layout = drive_layouts;
      SectorCount = Disk->SectorCount();
      SectorSize  = Disk->SectorSize(4);
      //
      // Check whether we find a layout that fits.
      while(layout->heads) { 
	ULONG seccount = layout->heads * layout->tracks * layout->secspertrack;
	if (layout->secsize      == SectorSize && SectorCount          == seccount) {
	  SectorsPerTrack = layout->secspertrack;
	  break; // found a match
	}
	layout++;
      }
      // Otherwise, make this a hard disk and assign it to a single track.
      if (layout->heads == 0)
	SectorsPerTrack = SectorCount;
    }
    //
    // Check for the sector size we find here to check the density.
    if (SectorSize == 512) {
      DiskStatus      = High;
    } else if (SectorSize == 256) {
      // This must be double density.
      DiskStatus   = Double;
    } else {
      if (SectorsPerTrack == 26) {
	DiskStatus = Enhanced;
      } else {
	DiskStatus = Single;
      }
    }
  } else {    
    if (errno == 0)
      Throw(InvalidParameter,"DiskDrive::OpenDiskFromStream","invalid disk image");
    else
      ThrowIo("DiskDrive::OpenDiskFromStream","unable to read the header of the disk image");
  }
}
///

/// DiskDrive::CheckCommandFrame
// Test whether a given command frame is acceptable and if so,
// return the type of the command. Return the size of its data frame.
SIO::CommandType DiskDrive::CheckCommandFrame(const UBYTE *commandframe,int &datasize)
{
  UWORD sector = UWORD(commandframe[2] | (commandframe[3] << 8));
  // If we are turned off, signal this.
  if (ProtectionStatus == Off)
    return SIO::Off;
  // We just check the command here.
  switch(commandframe[1]) {
  case 0x3f:  // Read speed byte (extended)
//this command is not exclusive to speedy (KMK)
//    if (EmulatesSpeedy) {
      datasize = 1;
      return SIO::ReadCommand;
//    } else {
//      return SIO::InvalidCommand;
//    }
  case 0x41:  // Set user definable command (extended)
    if (EmulatesSpeedy) {
      datasize = 3;  // three bytes: Command, address (lo,hi);
      return SIO::WriteCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0x44:  // set display control byte (extended)
    if (EmulatesSpeedy) {
      return SIO::StatusCommand; // wierd enough
    } else {
      return SIO::InvalidCommand;
    }
  case 0x4b:  // set speed control byte (extended)
    if (EmulatesSpeedy) {
      return SIO::StatusCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0x4c:  // jump w/o status (extended)
  case 0x4d:  // jump with status (extended)
    if (EmulatesSpeedy) {
      return JumpStatusType(sector,datasize);
    } else {
      return SIO::InvalidCommand;
    }
  case 0x4e:  // read geometry (extended)
    if (Emulates815) {
      datasize = 12;
      return SIO::ReadCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0x4f:  // write geometry (extended)
    if (Emulates815) {
      datasize = 12;
      return SIO::WriteCommand;
    } else {
      return SIO::InvalidCommand;
    }   
  case 0x51:  // write back cache (extended)
    if (EmulatesSpeedy) {
      return SIO::StatusCommand;
    } else {
      return SIO::InvalidCommand;
    } 
  case 0xd0:
  case 0xd7:  // doubler fast write?  
    if (!EmulatesSpeedy) {
      return SIO::InvalidCommand;
    }
    // Runs into the following...
  case 0x50:  // write w/o verify
  case 0x57:  // write with verify   
    if ((datasize = SpeedyBankSize(sector)))
      return SIO::WriteCommand;
    // True hardware sector size: Ask the disk otherwise.
    // If no disk, leave at zero.
    if (Disk && ProtectionStatus != UnLoaded) {
      datasize = Disk->SectorSize(sector);
    } else {
      datasize = SectorSize; // 128;
    }
    return SIO::WriteCommand;
  case 0x21:  // format single density
    datasize    = SectorSize; // Keep as defined by the status command
    // (DiskStatus == Double)?256:128; // This looks wierd, but seems to be correct!
    return SIO::FormatCommand;  
  case 0x22:  // format enhanced density  
    datasize      = 128;
    return SIO::FormatCommand;
  case 0x23:  // Start drive test
    datasize      = 128;
    return SIO::WriteCommand;
  case 0x24:  // Stop drive test, deliver the results.
    datasize      = 128;
    return SIO::ReadCommand;
  case 0xd2:  // doubler fast read?
  case 0x55:  // happy fast read? 
    if (!EmulatesSpeedy) {
      return SIO::InvalidCommand;
    }
    // Runs into the following...
  case 0x52:  // read
    if ((datasize = SpeedyBankSize(sector)))
      return SIO::ReadCommand;
    if (Disk && ProtectionStatus != UnLoaded) {
      datasize = Disk->SectorSize(sector);
    } else {
      datasize = SectorSize; //128;
    }
    return SIO::ReadCommand;
  case 0xd3:  // doubler fast status? 
    if (!EmulatesSpeedy) {
      return SIO::InvalidCommand;
    }
    // Runs into the following...
  case 0x53:  // Read Status
    datasize = 4;
    return SIO::ReadCommand;
  }

  return SIO::InvalidCommand;
}
///

/// DiskDrive::WriteBuffer
// Write the indicated data buffer out to the target device.
// Return 'C' if this worked fine, 'E' on error.
// Does not alter the size passed in since we work on the full
// buffer.
UBYTE DiskDrive::WriteBuffer(const UBYTE *commandframe,const UBYTE *buffer,int &size)
{
  UWORD  sector = UWORD(commandframe[2] | (commandframe[3] << 8));
  
  switch (commandframe[1]) { 
  case 0x41 :  
    return InstallUserCommand(buffer,size);
  case 0x4c:
  case 0x4d:  // These two are ignored here: We do not care about write jumped bytes
    return 'C';
  case 0x4f:  // Write Status Block
    return WriteStatusBlock(buffer,size);
  case 0x23:  // Start drive test.
    if (size == 128) {
      ULONG secspertrack = SectorsPerTrack;
      secspertrack >>= 1;
      RunningTest    = buffer[0];
      switch(RunningTest) {
      case 0x00:
      case 0x01:
      case 0x02:
	return 'C';
      case 0x03: // Step up
	if (LastSector <= SectorCount - secspertrack)
	  LastSector += secspertrack; // or something else that moves us out of track #0
	return 'C';
      case 0x04: // Step down.
	if (LastSector >= secspertrack)
	  LastSector -= secspertrack;
	return 'C';
      case 0x05: // Seek back to track #0.
	LastSector  = 1;
	return 'C';
      }
    }
    return 'E';
  case 0x50: 
  case 0x57: // Write with and without verify
  case 0xd0:
  case 0xd7: // Doubler commands
    // We check here special sector counts for the Speedy CPU RAM
    // access.
    {
      size_t sectorsize,sz = size;
      sectorsize = SpeedyBankSize(sector);
      if (sectorsize == 0) {
	// Ok, here we are dealing with a true disk sector. Perform the write.
	if (Disk && ProtectionStatus == ReadWrite) {
	  LastSector = sector;
	  sectorsize = Disk->SectorSize(sector);
	  if (sectorsize == sz) {
	    return Disk->WriteSector(sector,buffer);
	  }
	}
	// Signal an error in all other cases.
	return 'E';
      } else if (sectorsize == sz) {
	// Write into a speedy bank
	UBYTE *bank = SpeedyBank(sector);
	memcpy(bank,buffer,sectorsize);
	return 'C';
      }
    }
  }  
  machine->PutWarning("Unknown command frame: %02x %02x %02x %02x\n",
		      commandframe[0],commandframe[1],commandframe[2],commandframe[3]);

  return 'E';
}
///

/// DiskDrive::ReadBuffer
// Fill a buffer by a read command, return the amount of
// data read back (in bytes), not counting the checksum
// byte which is computed for us by the SIO.
// We do not alter the size passed in since we read the command
// completely.
UBYTE DiskDrive::ReadBuffer(const UBYTE *commandframe,UBYTE *buffer,int &)
{
  UWORD sector = UWORD(commandframe[2] | (commandframe[3] << 8));
  
  switch (commandframe[1]) {
  case 0x3f: // Read Speed Byte
    buffer[0]      = SpeedControl;
    return 'C';
  case 0x4c:
  case 0x4d:
    // This is "jump with status". We only emulate it partially.
    return JumpStatus(sector,buffer);
  case 0x4e: // Read Status
    return ReadStatusBlock(buffer);
  case 0x52: // Read (read command)  
  case 0x55: // happy fast read?
  case 0xd2: // doubler fast read?
    {
      size_t sectorsize = SpeedyBankSize(sector);
      if (sectorsize == 0) {
	// Is not a speedy bank, ask usual floppy
	if (Disk) {
	  LastSector = sector;
	  return Disk->ReadSector(sector,buffer);
	}
	// Return an error otherwise.
	return 'E';
      } else {
	// Otherwise copy the contents from the Speedy bank over
	UBYTE *bank = SpeedyBank(sector);
	memcpy(buffer,bank,sectorsize);
	return 'C';
      }
    }
  case 0x53: // Status (read command)
  case 0xd3: // Doubler fast read?
    return DriveStatus(buffer);
  case 0x21 : // single density format
    LastSector = 1;
    return FormatSingle(buffer,sector);
  case 0x22:
    LastSector = 1;
    return FormatEnhanced(buffer,sector);
  case 0x24: // Return drive test results.
    {
      UBYTE test  = RunningTest;
      memset(buffer,0,128);
      RunningTest = 0xff;
      //
      switch(test) {
      case 0x00: // Speed test result.
	buffer[0] = 0x20;
	buffer[1] = 0x08;
	return 'C';
      case 0x01: // Motor start test
	buffer[0] = 0x14;
	return 'C';
      case 0x02: // Head step/settle test.
      case 0x03:
      case 0x04:
      case 0x05:
	return 'C';
      default:
	// Unknown test
	return 'E';
      }
    }
  }
  //  
  machine->PutWarning("Unknown command frame: %02x %02x %02x %02x\n",
		      commandframe[0],commandframe[1],commandframe[2],commandframe[3]);
  // Return an error otherwise
  return 'E';
}
///

/// DiskDrive::ReadStatus
// Execute a status-only command that does not read or write any data except
// the data that came over AUX1 and AUX2
UBYTE DiskDrive::ReadStatus(const UBYTE *commandframe)
{
  switch(commandframe[1]) {  
  case 0x44: // Set Display control. This is a read command (Yuck!)
    DisplayControl = commandframe[2];
    return 'C';
  case 0x4b:  // set speed control byte (extended)
    SpeedControl   = commandframe[2];
    return 'C'; // does nothing
  case 0x4c:    // jump w/o status (extended)
    return 'C';
  case 0x4d:  // this command *may* send no return
    return 'C';
  case 0x51:  // write back cache (extended)
    return 'C'; // does nothing, there is no cache.
  }
  machine->PutWarning("Unknown command frame: %02x %02x %02x %02x\n",
		      commandframe[0],commandframe[1],commandframe[2],commandframe[3]);
  return 'N';
}
///

/// DiskDrive::SwitchPower
// Turn the drive on or off.
void DiskDrive::SwitchPower(bool onoff)
{
  if (onoff) {
    // Turn drive on. If it is on already, do nothing.
    if (ProtectionStatus == Off) {
      // Drive is off. Now turn it on.
      EjectDisk();
    }
  } else {
    // If the drive is off already, do nothing.
    if (ProtectionStatus != Off) {
      EjectDisk();
      ProtectionStatus = Off;
    }
  }
}
///

/// DiskDrive::EjectDisk
// If the drive is loaded with a disk, eject it.
void DiskDrive::EjectDisk(void)
{
  if (ProtectionStatus != Off) {
    // Unload all streams.
    ProtectionStatus = UnLoaded;
    delete Disk;
    Disk             = NULL;
    delete ImageStream;
    ImageStream      = NULL;
    delete[] ImageName;
    ImageName        = NULL;
    DiskStatus       = None;
    ImageType        = Unknown;
    ProtectionStatus = UnLoaded;
  }
}
///

/// DiskDrive::InsertDisk
// Load a disk, possibly throw on error, and write-protect the disk should we
// have one.
void DiskDrive::InsertDisk(bool protect)
{
  if (ImageToLoad) {
    LoadImage(ImageToLoad);
#if CHECK_LEVEL > 0
    if (ImageName)
      Throw(ObjectExists,"DiskDrive::InsertDisk","the inserted disk image name exists already");
#endif
    // Copy the name over from here.
    ImageName = new char[strlen(ImageToLoad) + 1];
    strcpy(ImageName,ImageToLoad);
    // Now check whether we should write-protect it.
    if (Disk && protect) {
      Disk->ProtectImage();
      //
      // This should hopefully work now.
      ProtectionStatus = Disk->ProtectionStatus()?(ReadOnly):(ReadWrite);
    }
  }
}
///

/// DiskDrive::ParseArgs
// Argument parser stuff: Parse off command line args
void DiskDrive::ParseArgs(class ArgParser *args)
{  
  char enableoption[32],imageoption[32],protectoption[32],drivemenu[32],ejectoption[32];
  char speedyoption[32],doubleoption[32];
  bool protect = (ProtectionStatus == ReadOnly);
  bool onoff   = (ProtectionStatus != Off);
  bool eject   = false;

  // generate the appropriate options now
  sprintf(enableoption,"Enable.%d",DriveId+1);
  sprintf(imageoption,"Image.%d",DriveId+1);
  sprintf(protectoption,"Protect.%d",DriveId+1);
  sprintf(ejectoption,"Eject.%d",DriveId+1);
  sprintf(drivemenu,"Drive.%d",DriveId+1);
  sprintf(speedyoption,"Happy.%d",DriveId+1);
  sprintf(doubleoption,"Emul815.%d",DriveId+1);

  if (DriveId == 0) {
    args->DefineTitle("DiskDrive");
    args->OpenSubItem("Disks");
  }
  if (ImageToLoad && *ImageToLoad && ProtectionStatus == UnLoaded) {
    eject = true;
  } 
  args->OpenSubItem(drivemenu);
  args->DefineBool(enableoption,"power the drive on",onoff);
  args->DefineBool(protectoption,"write protect the image file",protect); 
  args->DefineBool(ejectoption,"eject disk in the specified drive",eject);
  args->DefineBool(doubleoption,"enable 815 double density drive",Emulates815);
  args->DefineBool(speedyoption,"enable Speedy emulation",EmulatesSpeedy);
  args->DefineFile(imageoption,"load the drive with the specified image",ImageToLoad,true,true,false);
  
  if (onoff == false) {
    SwitchPower(false);
  } else {
    SwitchPower(true);
    if (ProtectionStatus == Off)
      ProtectionStatus = UnLoaded;
  }
  if ((eject || (ImageToLoad && *ImageToLoad)) && ProtectionStatus != Off) {
    // Avoid disk changes if possible.
    if (eject) {
      if (ImageName) {
	EjectDisk();
      }
    } else {
      if (ImageName == NULL || ImageToLoad      == NULL || strcmp(ImageToLoad,ImageName) ||
	  (protect  ==true  && ProtectionStatus == ReadWrite) ||
	  (protect  ==false && ProtectionStatus == ReadOnly)) {
	InsertDisk(protect);
      }
    }
  }
  args->CloseSubItem();
  // The next "DefineTitle" opens a new root item anyhow, no need to 
  // close this explicitly.
}
///   

/// DiskDrive::DisplayStatus
// Status display: Present details about the contents of the
// drive and other internals.
void DiskDrive::DisplayStatus(class Monitor *mon)
{
  const char *status;

  switch(ProtectionStatus) {
  case Off:
    status = "Off";
    break;
  case UnLoaded:
    status = "No disk inserted";
    break;
  case ReadOnly:
    status = "Read only";
    break;
  case ReadWrite:
    status = "Read/write";
    break;
  default:
    status = "Unknown";
  }

  mon->PrintStatus("Diskdrive D%d: status:\n"
		   "\tDiskStatus       : %s\n",
		   DriveId,status);

  if (ProtectionStatus == ReadWrite || ProtectionStatus == ReadOnly) {
    const char *disktype,*imagetype;
    // There is more to print
    switch(DiskStatus) {
    case None:
      disktype = "None"; // this should actually never happen
      break;
    case Single:
      disktype = "Single density";
      break;
    case Enhanced:
      disktype = "Enhanced density";
      break;
    case Double:
      disktype = "Double density";
      break;
    case High:
      disktype = "High density";
      break;
    default:
      disktype = "Unknown";
    }
    switch(ImageType) {
    case Unknown:
      imagetype = "Unknown"; // this should not happen
      break;
    case XFD:
      imagetype = "XFD";     // XFD format
      break;
    case ATR:
      imagetype = "ATR";     // ATR format
      break;
    case CMD:
      imagetype = "Binary Boot";
      break;
    case FILE:
      imagetype = "Program Source";
      break;
    case DCM:
      imagetype = "DCM";
      break;
    default:
      imagetype = "Undefined";
    }
    //
    mon->PrintStatus("\tImage file       : %s\n"
		     "\tDisk format      : %s\n"
		     "\tImage file format: %s\n"
		     "\tSectors          : " LU "\n"
		     "\tSector size      : %d\n"
		     "\tSectors per track: " LU "\n",
		     ImageName,disktype,imagetype,SectorCount,SectorSize,SectorsPerTrack);
  }
  mon->PrintStatus("\n");
}
///
