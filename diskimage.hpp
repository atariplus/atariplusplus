/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: diskimage.hpp,v 1.1 2003/01/26 18:16:21 thor Exp $
 **
 ** In this module: Abstract base class for disk images
 **********************************************************************************/

#ifndef DISKIMAGE_HPP
#define DISKIMAGE_HPP

/// Includes
#include "types.hpp"
#include "imagestream.hpp"
///

/// Forwards
class Machine;
///

/// Class DiskImage
// This class defines the abstract base class for all types of
// disk images. It is used by the diskdrive class to represent
// inserted disks.
class DiskImage {
protected:
  class Machine *Machine;
  //
public:
  DiskImage(class Machine *mach);
  virtual ~DiskImage(void);
  //
  // Open a disk image from a file given an image stream class.
  virtual void OpenImage(class ImageStream *image) = 0;
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector) = 0;
  // Return the number of sectors.
  virtual ULONG SectorCount(void) = 0;
  //
  // Return the protection status of this image. true if it is protected.
  virtual bool ProtectionStatus(void) = 0;
  //
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer) = 0;
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD sector,const UBYTE *buffer) = 0;
  //
  // Protect an image on user request
  virtual void ProtectImage(void) = 0;
};
///

///
#endif
