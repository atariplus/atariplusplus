/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: diskdrive.cpp,v 1.98 2022/01/08 21:55:43 thor Exp $
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
#include "atximage.hpp"
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
  : SerialDevice(mach,name,'1'+id), DriveId(id)
{
  DiskStatus       = None;
  // Default is to turn only drive #1 on
  ProtectionStatus = (id == 0)?(UnLoaded):(Off);
  ImageType        = Unknown;
  ImageStream      = NULL;
  Disk             = NULL;
  ImageName        = NULL;
  ImageToLoad      = NULL;
  SpeedControl     = SIO::Baud19200 - 7; // speedy default speed: that of a 1050 (in pokey timers, must add 7)
  //
  // Install drive default settings: 128 byte sectors, 720 of them.
  SectorSize       = 128;
  SectorCount      = 720;
  SectorsPerTrack  = 18;
  DriveModel       = Atari1050;
  //
  // Provide default configuration.
  LastFDCCommand   = FDC_Reset;
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
}
///

/// DiskDrive::ColdStart
void DiskDrive::ColdStart(void)
{
  LastFDCCommand = FDC_Reset;

  //
  // The disk image may require a reset signal to restore to its initial state.
  if (Disk)
    Disk->Reset();
}
///

/// DiskDrive::WarmStart
void DiskDrive::WarmStart(void)
{
  // As the disk drive is not connected to the
  // CPU RESET line, nothing can happen here.
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

  // Construct the FDC status - note that this is active-low.
  switch(LastFDCCommand) {
  case FDC_Read:
  case FDC_ReadTrack:
    // read track and read sector command always have the
    // write protection bit clear.
    buffer[1]   |= DiskImage::Protected | DiskImage::Busy;
    if (Disk && (ProtectionStatus == ReadOnly || ProtectionStatus == ReadWrite)) {
      buffer[1] |= ~Disk->Status();
      buffer[1] |= DiskImage::NotReady;
    } else {
      // RNF always clear, no record there.
      buffer[1] |= DiskImage::NotFound | DiskImage::CRCError; 
      buffer[1] |= DiskImage::LostData | DiskImage::DRQ;
    }
    break;
  case FDC_Write:
  case FDC_WriteTrack:
    // The FDC write protection bit is mirrored from the disk status.
    buffer[1]   |= DiskImage::Busy; 
    if (Disk && (ProtectionStatus == ReadOnly || ProtectionStatus == ReadWrite)) {
      buffer[1] |= ~Disk->Status();
      buffer[1] |= DiskImage::NotReady;
    } else {
      buffer[1] |= DiskImage::NotFound | DiskImage::CRCError;
      buffer[1] |= DiskImage::LostData | DiskImage::DRQ;
    }
    break;
  case FDC_Seek:
    // Seek commands, type I. 
    buffer[1]   |= DiskImage::Busy; 
    if (Disk && (ProtectionStatus == ReadOnly || ProtectionStatus == ReadWrite)) {
      // Copy write protection here.
      buffer[1] |= ~Disk->Status();
      buffer[1] |= DiskImage::NotReady;
      buffer[1] &= ~UBYTE(1 << 5); // head loaded, bit 5 has a different meaning.
    } else {
      buffer[1] |= DiskImage::CRCError;
    }
    buffer[1] |= 1 << 4; // no seek error, bit 4 has a different meaning.
    buffer[1] &= ~UBYTE(1 << 1); // index pulse detected.
    if (LastSector <= SectorsPerTrack) {
      buffer[1] |= 1 << 2; // reached track zero.
    } else {
      buffer[1] &= ~UBYTE(1 << 2); // outside of track zero.
    }
    break;
  case FDC_Reset:
    // I really do not know the state of the FDC status after reset. Just make
    // it say "all fine here".
    buffer[1] = 0xff;
    break;
  }
  
  if (ProtectionStatus == ReadOnly) {
    buffer[0] |= 8;   // drive is read-only
  }

  switch(DiskStatus) {
  case Single:
    break;
  case Enhanced:
    buffer[0]   |= 128;
    break;
  case Double:
  case High:
    buffer[0]   |= 32;
    break;
  default: // Shut up the compiler
    break;
  }
  
  buffer[2] = 0xe0; // drive format timeout in seconds
  buffer[3] = 0;    // unused

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
    } else if (SectorCount == 1040) {
      // Switch back to SD if it was ED.
      SectorCount = 720;
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
    } else if ((buffer[0] == 'F' && buffer[1] == 'U' && buffer[2] == 'J' && buffer[3] == 'I') ||
	       (buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F')) {
      // This is a CAS archive stream. Or a tape image that still requires decoding.
      // Both can be done through the CASStream which again builds on top of a tape image.
      delete ImageStream;
      ImageStream = NULL;
      ImageStream = new class CASStream(machine);
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
  UBYTE buffer[32];
  UWORD vtable[8];
#if CHECK_LEVEL > 0
  // Check whether the disk is currently unloaded.
  if (Disk)
    Throw(ObjectExists,"DiskDrive::OpenDiskFromStream","the old image is still loaded, eject it first");
#endif
  //
  // Read the first two bytes to identify the type of the image.
  errno = 0;
  if (ImageStream->Read(0,buffer,16)) {
    // Check for the type of the file here now.
    if (buffer[0] == 0x96 && buffer[1] == 0x02) {
      // This is an ATR image. Open it from here.
      Disk = new class ATRImage(machine);
      Disk->OpenImage(ImageStream);
      ImageType   = ATR;
    } else if (buffer[0] == 'A' && buffer[1] == 'T') {
      // This is an ATX image.
      Disk = new class ATXImage(machine);
      Disk->OpenImage(ImageStream);
      ImageType   = ATX;
    } else if (buffer[0] == 0xff && buffer[1] == 0xff) {
      // This is a binary load file.
      Disk = new class BinaryImage(machine);
      Disk->OpenImage(ImageStream);
      ImageType   = CMD;
    } else if (buffer[0] == 0x00 && buffer[1] == 0x00 &&
	       ((vtable[1] = buffer[ 2] | (buffer[ 3] << 8)),
		(vtable[2] = buffer[ 4] | (buffer[ 5] << 8)),
		(vtable[3] = buffer[ 6] | (buffer[ 7] << 8)),
		(vtable[4] = buffer[ 8] | (buffer[ 9] << 8)),
		(vtable[5] = buffer[10] | (buffer[11] << 8)),
		(vtable[6] = buffer[12] | (buffer[13] << 8))) &&
	       vtable[1] > 0 && vtable[2] >= vtable[1] && vtable[3] >= vtable[2] &&
	       vtable[4] >= vtable[3] && vtable[5] >= vtable[4] && vtable[6] >= vtable[5]) {
      // This is a basic file.
      Disk = new class StreamImage(machine,"AUTORUN.BAS");
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
    ProtectionStatus = (Disk->Status() & DiskImage::Protected)?(ReadOnly):(ReadWrite);
    LastFDCCommand   = FDC_Reset;
    //    
    // Fill in sector size and sector count since we need it later
    // for the drive status anyhow.
    {
      const struct DriveLayout *layout = drive_layouts;
      ULONG bestguess = SectorCount; // Hard disk guess: A single track.
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
	if (layout->secsize      == SectorSize && SectorCount          <= seccount) {
	  bestguess       = layout->secspertrack;
	  break; // found a near match for an incomplete image.
	}
	layout++;
      }
      // Otherwise, use the best guess available.
      if (layout->heads == 0)
	SectorsPerTrack = bestguess;
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

/// DiskDrive::CheckDiskCompatibility
// Check whether the current disk format is actually supported
// by this drive type. If not, warn and eject the disk.
// Returns an appropriate warning if not compatible.
const char *DiskDrive::CheckDiskCompatibility(void)
{
  const char *warning = NULL;

  if (ProtectionStatus == ReadOnly || ProtectionStatus == ReadWrite) {
    switch(DriveModel) {
    case Atari810:
      if (SectorSize > 128) {
	warning = "The Atari 810 does not support double or high density disks";
      } else if (SectorCount > 720) {
	warning = "Atari 810 disks cannot hold more than 720 sectors";
      }
      break;
    case Atari815:
      if (SectorSize > 256) {
	warning = "The Atari 810 does not support high density disks";
      } else if (SectorCount > 720) {
	warning = "Atari 815 disks cannot hold more than 720 sectors";
      }
      break;
    case Atari1050:
      if (SectorSize > 128) {
	warning = "The Atari 1050 does not support double or high density disks";
      } else if (SectorCount > 1040) {
	warning = "Atari 1050 disks cannot hold more than 1040 sectors";
      }
      break;
    default:
      // No further checks.
      break;
    }
  }

  return warning;
}
///

/// DiskDrive::CheckCommandFrame
// Test whether a given command frame is acceptable and if so,
// return the type of the command. Return the size of its data frame.
SIO::CommandType DiskDrive::CheckCommandFrame(const UBYTE *commandframe,int &datasize,UWORD speed)
{
  UWORD sector = UWORD(commandframe[2] | (commandframe[3] << 8));
  // If we are turned off, signal this.
  if (ProtectionStatus == Off)
    return SIO::Off;
  //
  // Ok, check the transfer speed. Some drives do not react on all speeds.
  switch(DriveModel) {
  case Atari810:
  case Atari815:
  case Atari1050:
  case Happy810:  // The 1050 happy also only reacts on standard speed in the command frame.
  case IndusGT:
    // Only the standard speed.
    if (speed != SIO::Baud19200)
      return SIO::Off; // Do not react.
    break;
  default:
    // Only the standard speed, and the current speed.
    if (speed != SIO::Baud19200 && speed != SpeedControl + 7)
      return SIO::Off; // Do not react.
  }
  //
  // We just check the command here.
  switch(commandframe[1]) {
  case 0x3f:  // Read speed byte (extended)
    if (DriveModel == Happy1050 || DriveModel == Speedy) {
      datasize = 1;
      return SIO::ReadCommand;
    } else {
      // The remaining cannot....
      return SIO::InvalidCommand;
    }
  case 0x44:  // set display control byte
    if (Speedy == Happy810) {
      return SIO::StatusCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0x4b:  // set speed control byte (extended)
    if (Speedy == Happy810) {
      return SIO::StatusCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0x4e:  // read geometry (extended)
    if (DriveModel >= Happy1050 || DriveModel == Atari815) {
      datasize = 12;
      return SIO::ReadCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0x4f:  // write geometry (extended)
    if (DriveModel >= Happy1050 || DriveModel == Atari815) {
      datasize = 12;
      return SIO::WriteCommand;
    } else {
      return SIO::InvalidCommand;
    }   
  case 0x51:  // write back cache (extended) Unclear which drives support this.
    if (DriveModel == Happy1050 || DriveModel == Speedy) {
      return SIO::StatusCommand;
    } else {
      return SIO::InvalidCommand;
    } 
  case 0xd0:
  case 0xd7:  // XF551 fast write
  case 0x70:
  case 0x77:  // Warp Speed fast write
  case 0x50:  // write w/o verify
  case 0x57:  // write with verify   
    if ((commandframe[1] & 0x80) && DriveModel != XF551 && DriveModel != IndusGT)
      return SIO::InvalidCommand;
    if ((commandframe[1] & 0x20) && DriveModel != Happy810)
      return SIO::InvalidCommand; 
    if (DriveModel == USTurbo)
      sector &= 0x7fff; // remove high speed indicator.
    // True hardware sector size: Ask the disk otherwise.
    LastFDCCommand = FDC_Write;
    // If no disk, leave at zero.
    if (Disk && ProtectionStatus != UnLoaded) {
      datasize = Disk->SectorSize(sector);
    } else {
      datasize = SectorSize; // 128;
    }
    return SIO::WriteCommand;
  case 0x20:  // Format auto. Speedy only.
    if (DriveModel == Speedy) {
      datasize       = SectorSize;
      LastFDCCommand = FDC_WriteTrack;
      return SIO::FormatCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0xa1:
  case 0x21:  // format single density, supported by all drives. 
    if ((commandframe[1] & 0x80) && DriveModel != XF551 && DriveModel != IndusGT)
      return SIO::InvalidCommand;
    datasize       = SectorSize; // Keep as defined by the status command
    LastFDCCommand = FDC_WriteTrack;
    return SIO::FormatCommand;  
  case 0xa2:
  case 0x22:  // format enhanced density  
    if ((commandframe[1] & 0x80) && DriveModel != XF551 && DriveModel != IndusGT)
      return SIO::InvalidCommand;
    if (DriveModel >= Atari1050) {
      datasize       = 128;
      LastFDCCommand = FDC_WriteTrack;
      return SIO::FormatCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0x23:  
    // Start drive test: 1050 only. 
    // Maybe the diags are supported by other drive types as well?
    if (DriveModel == Atari1050) {
      datasize       = 128;
      LastFDCCommand = FDC_Seek;
      return SIO::WriteCommand;
    } else if (DriveModel == XF551 || DriveModel == IndusGT) {
      // Very smart to use the diag command to format a disk.... Oh well.
      datasize       = SectorSize;
      LastFDCCommand = FDC_WriteTrack;
      return SIO::FormatCommand;  
    } else {
      return SIO::InvalidCommand;
    }
  case 0x24:  
    // Stop drive test, deliver the results: 
    // Also 1050 only, maybe supported by some other drives?
    if (DriveModel == Atari1050) {
      datasize      = 128;
      return SIO::ReadCommand;
    } else {
      return SIO::InvalidCommand;
    }
  case 0xd2:  // doubler fast read
  case 0x72:  // Warp fast read
  case 0x52:  // regular read
    if ((commandframe[1] & 0x80) && DriveModel != XF551 && DriveModel != IndusGT)
      return SIO::InvalidCommand;
    if ((commandframe[1] & 0x20) && DriveModel != Happy810)
      return SIO::InvalidCommand;
    if (DriveModel == USTurbo)
      sector &= 0x7fff; // remove high speed indicator.
    LastFDCCommand = FDC_Read;
    if (Disk && ProtectionStatus != UnLoaded) {
      datasize = Disk->SectorSize(sector);
    } else {
      datasize = SectorSize;
    }
    return SIO::ReadCommand;
  case 0xd3:
  case 0x73:
  case 0x53: // all variations of the status command
    if ((commandframe[1] & 0x80) && DriveModel != XF551 && DriveModel != IndusGT)
      return SIO::InvalidCommand;
    if ((commandframe[1] & 0x20) && DriveModel != Happy810)
      return SIO::InvalidCommand;
    datasize = 4;
    return SIO::ReadCommand;
  case 0x48: // Happy enable high speed mode.
    if (DriveModel == Happy1050 || DriveModel == Happy810) {
      return SIO::StatusCommand;
    } else {
      return SIO::InvalidCommand;
    }
  }
  // TODO: Implement the double-sided mode ("set large mode") of the 1450XLD,
  // and the corresponding SetSmallMode. Commands 1,2, respectively.
  // Implement custom format of the US Doubler (0x66). Data is the PERCOM block
  // plus the sector skew table.

  return SIO::InvalidCommand;
}
///

/// DiskDrive::AcknowledgeCommandFrame
// Acknowledge the command frame. This is called as soon the SIO implementation
// in the host system tries to receive the acknowledge function from the
// client. Will return 'A' in case the command frame is accepted. Note that this
// is only called if CheckCommandFrame indicates already a valid command.
UBYTE DiskDrive::AcknowledgeCommandFrame(const UBYTE *,UWORD &,UWORD &speed)
{
  UWORD cur = speed;
  // USTurbo responds in high speed, so does the Happy1050. Everything
  // else requires standard speed.
  switch(DriveModel) {
  case USTurbo:
  case Happy1050: 
  case Speedy:
    // Both works!
    return 'A';
  default:
    speed = SIO::Baud19200;
    if (cur != speed)
      return 'N';
    return 'A';
  }
}
///

/// DiskDrive::WriteBuffer
// Write the indicated data buffer out to the target device.
// Return 'C' if this worked fine, 'E' on error.
// Does not alter the size passed in since we work on the full
// buffer.
UBYTE DiskDrive::WriteBuffer(const UBYTE *commandframe,const UBYTE *buffer,
			     int &size,UWORD &delay,UWORD speed)
{
  UWORD  sector = UWORD(commandframe[2] | (commandframe[3] << 8));
  //
  // Ok, check the transfer speed. Some drives do not react on all speeds.
  switch(DriveModel) {
  case Atari810:
  case Atari815:
  case Atari1050:
    // Only the standard speed.
    if (speed != SIO::Baud19200)
      return SIO::Off; // Do not react.
    break;
  default:
    // Only the standard speed, and the current speed.
    if (speed != SIO::Baud19200 && speed != SpeedControl + 7)
      return SIO::Off; // Do not react.
  } 
  //
  switch (commandframe[1]) { 
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
  case 0x70:
  case 0x77:
    if (DriveModel == USTurbo)
      sector &= 0x7fff; // remove high speed indicator.
    if (Disk && ProtectionStatus == ReadWrite) {
      UWORD sectorsize  = Disk->SectorSize(sector);
      LastSector  = sector;
      if (sectorsize == size) {
	return Disk->WriteSector(sector,buffer,delay);
      }
    }
    // Signal an error in all other cases.
    return 'E';
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
UBYTE DiskDrive::ReadBuffer(const UBYTE *commandframe,UBYTE *buffer,int &,UWORD &delay,UWORD &speed)
{
  UWORD sector = UWORD(commandframe[2] | (commandframe[3] << 8));
  //
  // Ok, check the transfer speed. Some drives do not react on all speeds.
  switch(DriveModel) {
  case Atari810:
  case Atari815:
  case Atari1050:
    // Only the standard speed.
    speed = SIO::Baud19200;
    break;
  default:
    // Only the standard speed, and the current speed.
    break;
  } 
  //  
  switch (commandframe[1]) {
  case 0x3f: // Read Speed Byte
    buffer[0]      = SpeedControl;
    return 'C';
  case 0x4e: // Read Status
    return ReadStatusBlock(buffer);
  case 0x72:
  case 0xd2: // doubler fast read
    speed = SpeedControl + 7;
    // Falls through.
  case 0x52: // Read (read command)  
    if (DriveModel == USTurbo && (sector & 0x8000)) {
      sector &= 0x7fff; // remove high speed indicator.
      speed   = SpeedControl + 7;
    }
    if (Disk) {
      LastSector  = sector;
      return Disk->ReadSector(sector,buffer,delay);
    }
    // Return an error otherwise.
    return 'E';
  case 0xd3: // Doubler fast read?
  case 0x73:
    speed = SpeedControl + 7;
    // Falls through.
  case 0x53: // Status (read command)
    return DriveStatus(buffer);
  case 0xa1:
    speed = SpeedControl + 7;
    // Falls through.
  case 0x21 : // single density format
  case 0x20 : // Speedy format auto.
  case 0x23 : // Indus and XF551 format with sector skew command.
    if (DriveModel == USTurbo && (sector & 0x8000)) {
      sector &= 0x7fff; // remove high speed indicator.
      speed   = SpeedControl + 7;
    }
    LastSector = 1;
    return FormatSingle(buffer,sector);
  case 0xa2:
    speed = SpeedControl + 7;
    // Falls through.
  case 0x22:
    if (DriveModel == USTurbo && (sector & 0x8000)) {
      sector &= 0x7fff; // remove high speed indicator.
      speed   = SpeedControl + 7;
    }
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
	buffer[1] = 0x23;
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
UBYTE DiskDrive::ReadStatus(const UBYTE *commandframe,UWORD &,UWORD &speed)
{ 
  // Ok, check the transfer speed. Some drives do not react on all speeds.
  switch(DriveModel) {
  case Atari810:
  case Atari815:
  case Atari1050:
    // Only the standard speed.
    speed = SIO::Baud19200;
    break;
  default:
    // Only the standard speed, and the current speed.
    break;
  } 
  //
  switch(commandframe[1]) {  
  case 0x44:  // Set Display control.
    return 'C';
  case 0x51:  // write back cache (extended)
    return 'C'; // does nothing, there is no cache.
  case 0x48:  // Enable high speed mode.
    return 'C';
  case 0x4b:  // Happy slow/fast config.
    return 'C';
  case 0x55:  // Motor on
    return 'C';
  case 0x56:  // Verify sector
    return 'C';
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
    LastFDCCommand   = FDC_Reset;
    SectorSize       = 128;
    SectorCount      = 720;
  }
}
///

/// DiskDrive::InsertDisk
// Load a disk, possibly throw on error, and write-protect the disk should we
// have one.
void DiskDrive::InsertDisk(bool protect)
{
  if (ImageToLoad && *ImageToLoad) {
    LoadImage(ImageToLoad);
#if CHECK_LEVEL > 0
    if (ImageName)
      Throw(ObjectExists,"DiskDrive::InsertDisk","the inserted disk image name exists already");
#endif
    // Copy the name over from here.
    ImageName      = new char[strlen(ImageToLoad) + 1];
    LastFDCCommand = FDC_Reset;
    strcpy(ImageName,ImageToLoad);
    // Now check whether we should write-protect it.
    if (Disk && protect) {
      Disk->ProtectImage();
      //
      // This should hopefully work now.
      ProtectionStatus = (Disk->Status() & DiskImage::Protected)?(ReadOnly):(ReadWrite);
    }
  } else {
    EjectDisk();
  }
}
///

/// DiskDrive::ParseArgs
// Argument parser stuff: Parse off command line args
void DiskDrive::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector drivetypevector[] =
    { {"1050",           Atari1050 },
      {"810",            Atari810  },
      {"815",            Atari815  },
      {"Happy1050",      Happy1050 },
      {"WarpSpeed810",   Happy810  },
      {"Speedy",         Speedy    },
      {"XF551",          XF551     },
      {"USTurbo",        USTurbo   },
      {"IndusGT",        IndusGT   },
      {NULL,             0         }
    };
  char enableoption[32],imageoption[32],protectoption[32],drivemenu[32];
  char typeoption[32];
  char speedoption[32];
  bool protect   = (ProtectionStatus == ReadOnly);
  bool onoff     = (ProtectionStatus != Off);
  LONG drivetype = DriveModel;
  LONG speed     = SpeedControl;
  LONG newspeed  = -1;
  const char *warning = NULL;

  // generate the appropriate options now
  sprintf(enableoption,"Enable.%d",DriveId+1);
  sprintf(imageoption,"Image.%d",DriveId+1);
  sprintf(protectoption,"Protect.%d",DriveId+1);
  sprintf(drivemenu,"Drive.%d",DriveId+1);
  sprintf(typeoption,"DriveModel.%d",DriveId+1);

  if (DriveId == 0) {
    args->DefineTitle("DiskDrive");
    args->OpenSubItem("Disks");
  }
  args->OpenSubItem(drivemenu);
  args->DefineFile(imageoption,"load the drive with the specified image",ImageToLoad,true,true,false);
  args->DefineBool(enableoption,"power the drive on",onoff);
  args->DefineBool(protectoption,"write protect the image file",protect); 
  args->DefineSelection(typeoption,"disk drive type and features",drivetypevector,drivetype);
  if (drivetype != LONG(DriveModel)) {
    //
    // Reset the speed to the default speed
    switch(drivetype) {
    case Atari810:
    case Atari815:
    case Atari1050:
      newspeed = 40;
      break;
    case Happy1050:
      newspeed = 10;
      break;
    case Speedy:
      newspeed = 9;
      break;
    case Happy810:
    case XF551:
      newspeed = 16;
      break;
    case USTurbo:
    case IndusGT:
      newspeed = 6;
      break;
    }
    speed = SpeedControl = UBYTE(newspeed);
  }
  switch(drivetype) {
  case Happy1050:
  case Happy810:
  case Speedy:
  case XF551:
  case USTurbo:
  case IndusGT:
    sprintf(speedoption,"%sSpeed.%d",drivetypevector[drivetype].Name,DriveId+1);
    args->DefineLong(speedoption,"serial transfer speed",2,40,speed);
    break;
  default:
    // These drives are not configurable. The speed is fixed at the classical pokey divisor of 40
    speed = 40;
    break;
  }
  args->CloseSubItem();  
  if (newspeed >= 0) {
    speed = SpeedControl = UBYTE(newspeed);
    args->SignalBigChange(ArgParser::Reparse);
  }
  // The next "DefineTitle" opens a new root item anyhow, no need to 
  // close this explicitly.
  DriveModel   = DriveType(drivetype);
  SpeedControl = UBYTE(speed);
  
  if (onoff == false) {
    SwitchPower(false);
  } else {
    SwitchPower(true);
    if (ProtectionStatus == Off)
      ProtectionStatus = UnLoaded;
  }
  if (ProtectionStatus != Off) {
    if (ImageName == NULL || ImageToLoad      == NULL || strcmp(ImageToLoad,ImageName) ||
	(protect  ==true  && ProtectionStatus == ReadWrite) ||
	(protect  ==false && ProtectionStatus == ReadOnly)) {
      InsertDisk(protect);
    }
  }

  warning = CheckDiskCompatibility();
  if (warning) {
    //machine->PutWarning("Disk format is not supported natively: %s",warning);
    EjectDisk();
    delete[] ImageToLoad;
    ImageToLoad      = NULL;
    throw AtariException("unsupported disk format","DiskDrive::ParseArgs","%s",warning);
  }
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

  //
  // Extract the FDC status.
  if (ProtectionStatus != Off && Disk) { 
    UBYTE status = Disk->Status();

    mon->PrintStatus("\tFDC Status       : ");

    if (status & DiskImage::LostData)
      mon->PrintStatus("lost data ");
    if (status & DiskImage::CRCError)
      mon->PrintStatus("CRC error ");
    if (status & DiskImage::NotFound)
      mon->PrintStatus("sector missing ");
    if ((status & (DiskImage::LostData | DiskImage::CRCError |
		   DiskImage::NotFound | DiskImage::Protected)) == 0)
      mon->PrintStatus("OK");
    mon->PrintStatus("\n");
  }

  if (ProtectionStatus == ReadWrite || ProtectionStatus == ReadOnly) {
    const char *disktype,*imagetype,*drivetype;
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
    case ATX:
      imagetype = "ATX";     // ATX format
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
    switch(DriveModel) {
    case Atari810:
      drivetype = "Atari 810";
      break;
    case Atari815:
      drivetype = "Atari 815";
      break;
    case Atari1050:
      drivetype = "Atari 1050";
      break;
    case Happy1050:
      drivetype = "Happy 1050";
      break;
    case Happy810:
      drivetype = "Warp Speed 810";
      break;
    case Speedy:
      drivetype = "Speedy";
      break;
    case XF551:
      drivetype = "XF551";
      break;
    case USTurbo:
      drivetype = "US Turbo";
      break;
    case IndusGT:
      drivetype = "Indus GT";
      break;
    default:
      drivetype = "Invalid";
      break;
    }
    //
    mon->PrintStatus("\tDrive model      : %s\n"
		     "\tImage file       : %s\n"
		     "\tDisk format      : %s\n"
		     "\tImage file format: %s\n"
		     "\tSectors          : " ATARIPP_LU "\n"
		     "\tSector size      : %d\n"
		     "\tSectors per track: " ATARIPP_LU "\n",
		     drivetype,ImageName,disktype,imagetype,SectorCount,SectorSize,SectorsPerTrack);
  }
  mon->PrintStatus("\n");
}
///
