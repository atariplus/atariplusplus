/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: streamimage.hpp,v 1.1 2009-08-10 13:08:25 thor Exp $
 **
 ** In this module: Disk image class for any type of files that need to be put on disk
 **********************************************************************************/

#ifndef STREAMIMAGE_HPP
#define STREAMIMAGE_HPP

/// Includes
#include "types.hpp"
#include "imagestream.hpp"
#include "diskimage.hpp"
///

/// Forwards
class Machine;
///

/// Class StreamImage
// This class defines an disk image for any type of files.
// A DOS 2.0S compatible disk structure is created for
// such files.
class StreamImage : public DiskImage {
  //
  //
  // Contents of the emulated disk goes here.
  UBYTE                 *Contents;
  class  ImageStream    *Image;
  ULONG                  ByteSize;
  //
  // Name of the file to be created.
  const char            *Name;
  //
public:
  StreamImage(class Machine *mach,const char *name);
  virtual ~StreamImage(void);
  //
  // Open a disk image from a file given an image stream class.
  virtual void OpenImage(class ImageStream *image);
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD)
  {
    return 128;
  }
  // Return the number of sectors.
  virtual ULONG SectorCount(void)
  {
    return ByteSize >> 7; // 128 byte sectors.
  }
  //
  // Return the protection status of this image. true if it is protected.
  virtual bool  ProtectionStatus(void)
  {
    return true;
  }
  //
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer);
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD,const UBYTE *)
  {
    // We cannot write to these files. Just return an error.
    return 'E';
  }
  // 
  // Protect an image on user request
  virtual void ProtectImage(void)
  {
  }
};
///

///
#endif
