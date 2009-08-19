/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: binaryimage.hpp,v 1.4 2009-08-10 13:08:25 thor Exp $
 **
 ** In this module: Disk image class for binary load files
 **********************************************************************************/

#ifndef BINARYIMAGE_HPP
#define BINARYIMAGE_HPP

/// Includes
#include "types.hpp"
#include "imagestream.hpp"
#include "diskimage.hpp"
///

/// Forwards
class Machine;
class ChoiceRequester;
///

/// Class BinaryImage
// This class defines an disk image for binary-load files. We
// prepend to this image a binary-load mini-dos to boot off
// the image without fuzz.
class BinaryImage : public DiskImage {
  //
  // Disk bootblock for booting off a binary load file.
  static const UBYTE     BootImage[];
  //
  //
  // Contents of the emulated disk goes here.
  UBYTE                 *Contents;
  class  ImageStream    *Image;
  ULONG                  ByteSize;
  //
  // A requester we pop up for error conditions.
  class ChoiceRequester *FixupRequester;
  //
  // The following structure keeps a byte/sector offset
  // and reads from an "image" file.
  struct FilePointer {
    UBYTE *Sector;       // pointer to current sector
    UBYTE  ByteOffset;   // byte offset in the current sector.
    //
    UBYTE Get(void); // Read from the image sectored.
    UWORD GetWord(void); // Read a word from the above, LO, HI.
    void Put(UBYTE data);
    void PutWord(UWORD data);
    //
    // Check whether we are at the EOF.
    bool Eof(void) const;
    //
    // Truncate the file at the current position.
    void Truncate(void);
    //
    // Setup the file pointer from an image
    FilePointer(UBYTE *src);
    // Empty constructor
    FilePointer(void)
    { }
  };
  //
  // Check whether the loaded image is setup fine.
  void VerifyImage(UBYTE *src);
public:
  BinaryImage(class Machine *mach);
  virtual ~BinaryImage(void);
  //
  // Open a disk image from a file given an image stream class.
  virtual void OpenImage(class ImageStream *image);
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector);
  // Return the number of sectors.
  virtual ULONG SectorCount(void);
  //
  // Return the protection status of this image. true if it is protected.
  virtual bool  ProtectionStatus(void);
  //
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer);
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD sector,const UBYTE *buffer);
  // 
  // Protect an image on user request
  virtual void ProtectImage(void);  
};
///

///
#endif
