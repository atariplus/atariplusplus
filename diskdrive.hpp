/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: diskdrive.hpp,v 1.36 2013/05/31 22:08:00 thor Exp $
 **
 ** In this module: Support for the serial (external) disk drive.
 **********************************************************************************/

#ifndef DISKDRIVE_HPP
#define DISKDRIVE_HPP

/// Includes
#include "types.hpp"
#include "serialdevice.hpp"
///

/// Forward references
class Monitor;
class DiskImage;
class ImageStream;
///

/// Class DiskDrive
// This drive emulates an enhanced 1050 Atari disk drive
class DiskDrive : public SerialDevice {
  //
#ifdef HAS_MEMBER_INIT
  static const int MaxUserCommands = 16; // number of definable user commands
#else
#define            MaxUserCommands   16
#endif
  //
  // Specification of the drive contents
  enum DiskType {
    None,                 // no disk inserted
    Single,               // single density, 720 sectors, 128 bytes/sector
    Enhanced,             // enhanced density, 1040 sectors, 128 bytes/sector
    Double,               // double density, 720 sectors, 256 bytes/sector
    High                  // high density, 512 bytes/sector
  }                      DiskStatus;
  //
  // Specification of the disk/drive status
  enum ProtectionType {
    Off,                  // drive is turned off
    UnLoaded,             // no disk in drive
    ReadOnly,             // cannot write 
    ReadWrite             // read and write
  }                      ProtectionStatus;
  //
  // Type of the image file, if available.
  enum ImageFileType {
    Unknown,              // not available
    XFD,                  // raw disk image
    ATR,                  // raw disk image with header
    ATX,                  // raw extended disk image
    CMD,                  // emulated command floppy image
    DCM,                  // DCM image
    FILE                  // Any type of source file
  }                      ImageType;
  //
  // Drive number of this drive.
  UBYTE                  DriveId;   // Indicates the drive number: 0..7
  //
  // 
  class ImageStream     *ImageStream; // where to take the data from.
  class DiskImage       *Disk;        // the inserted disk itself.
  char                  *ImageName;   // path of the disk image.
  char                  *ImageToLoad; // image we have to insert.
  UWORD                  SectorSize;
  ULONG                  SectorCount; // drive settings for the Atari 815 standard
  ULONG                  SectorsPerTrack;
  //
  // Running drive test, if any. This uses the
  // same indicator as the internal '#' command of the 1050.
  UBYTE                  RunningTest;
  //
  // The last sector we accessed.
  ULONG                  LastSector;
  //
  // Various internal buffers we might possibly need for "Speedy" emulation.
  // They are not allocated unless used. A future more complete emulation would
  // possibly add a CPU emulation on top of, but this is currently going over
  // the edge.
  UBYTE                 *Bank0000;
  UBYTE                 *Bank8000;
  UBYTE                 *BankC000;
  UBYTE                 *BankE000;
  // For Speedy only: Display control byte and Speed Control Byte
  UBYTE                  DisplayControl;
  UBYTE                  SpeedControl;
  //  
  // The last command issued. The command type defines how the
  // FDC control byte is read.
  enum CommandType {
    FDC_Reset,
    FDC_Seek,  // all FDC commands that move the head, command type I
    FDC_Read,  // read sector, command type II
    FDC_Write, // write sector, command type II
    FDC_ReadTrack,  // read track, command type III
    FDC_WriteTrack  // write track, command type III
  }                      LastFDCCommand;
  //
  // Keeps user floppy commands for "Happy"s (FIXME: Not yet implmented)
  struct FloppyCmd {
    UBYTE                CmdChar;   // The command character
    ADR                  Procedure; // what is to be called
  }                      UserCommands[MaxUserCommands];
  //
  // Configuration options:
  //
  // Enable 815 extended commands.
  bool                   Emulates815; 
  //
  // Enable Speedy/Happy commands.
  bool                   EmulatesSpeedy;
  //
  // Check whether this device is responsible for the indicated command frame
  // We must overload this method as the disk device exists twice.
  // The AtariSIO also handles the device and wants to get the chance
  // to intercept.
  virtual bool HandlesFrame(const UBYTE *commandframe)
  {
    if (ProtectionStatus != Off) {
      return SerialDevice::HandlesFrame(commandframe);
    }
    return false;
  }
  //
  // Private methods follow here
  //
  // For Happy's only: Install a user definable command
  // FIXME: This is not yet implemented as it would require
  // a detailed knowledge of the Happy hardware and a CPU
  // emulation on top. The latter, at least, would be easy.
  UBYTE InstallUserCommand(const UBYTE *buffer,int size);
  //
  // For speedy only: Return the command type a jump status implements
  SIO::CommandType JumpStatusType(ADR position,int &datasize);
  // For Speedy only: Jump to a Happy subroutine returning a status.
  UBYTE JumpStatus(ADR position,UBYTE *buffer);
  // For Speedy only: Get the size of a memory bank
  int SpeedyBankSize(ADR position);
  // Return the position within the bank
  UBYTE *SpeedyBank(ADR sector);
  //
  // Read the disk geometry and fill it into the buffer
  // Only for extended drives
  UBYTE ReadStatusBlock(UBYTE *buffer);
  //
  // Define some disk drive geometry data here
  // This is only for enhanced/extended drives
  UBYTE WriteStatusBlock(const UBYTE *buffer,int size);
  //
  //
  // Return the drive status bytes (four bytes)
  UBYTE DriveStatus(UBYTE *buffer);
  //
  // Emulate formatting a disk in single density
  // (or 720 sectors if you'd say so)
  // Fill the status bytes into the buffer and return the
  // size of the generated status.
  UBYTE FormatSingle(UBYTE *buffer,UWORD aux);
  //
  // Emulate formatting a disk in enhanced density
  // (or 1040 sectors if you'd say so)
  // Fill the status bytes into the buffer and return the
  // size of the generated status.
  UBYTE FormatEnhanced(UBYTE *buffer,UWORD aux);
  //
  // Create a new streaming image to write data to and return the SIO status
  UBYTE CreateNewImage(UWORD sectorsize,ULONG sectorcount);
  //
  // Load the disk drive with a new image of the given name, do not
  // keep that name. This is the low-level disk-insert method.
  // Returns a boolean success indicator
  void LoadImage(const char *filename);
  //
  // Open/Create a disk from a given ImageStream,
  // checking its type and building the proper type of
  // disk from it.
  void OpenDiskFromStream(void);
  //
public:
  // Constructors and destructors
  DiskDrive(class Machine *mach,const char *name,int id);
  ~DiskDrive(void);
  //
  // Methods required by SIO  
  // Check whether this device accepts the indicated command
  // as valid command, and return the command type of it.
  virtual SIO::CommandType CheckCommandFrame(const UBYTE *CommandFrame,int &datasize);
  //
  // Read bytes from the device into the system.
  virtual UBYTE ReadBuffer(const UBYTE *CommandFrame,UBYTE *buffer,
			   int &datasize,UWORD &delay);
  //  
  // Write the indicated data buffer out to the target device.
  // Return 'C' if this worked fine, 'E' on error.
  virtual UBYTE WriteBuffer(const UBYTE *CommandFrame,const UBYTE *buffer,
			    int &datasize,UWORD &delay);
  //
  // Execute a status-only command that does not read or write any data except
  // the data that came over AUX1 and AUX2
  virtual UBYTE ReadStatus(const UBYTE *CommandFrame,UWORD &delay);
  //
  // Other methods imported by the SerialDevice class:
  //
  // ColdStart and WarmStart
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Miscellaneous drive management stuff:
  //
  // Turn the drive on or off.
  void SwitchPower(bool onoff);
  //
  // Eject a loaded image
  void EjectDisk(void);
  //
  // Load a disk, possibly throw on error, and write-protect the disk should we
  // have one.
  void InsertDisk(bool protect);
  //
  // Argument parser stuff: Parse off command line args
  virtual void ParseArgs(class ArgParser *args);
  //
  // Status display
  virtual void DisplayStatus(class Monitor *mon);
  //
};
///

///
#endif
