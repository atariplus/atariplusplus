/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xfdimage.hpp,v 1.5 2015/07/14 20:01:00 thor Exp $
 **
 ** In this module: Disk image class for .xfd images.
 **********************************************************************************/

#ifndef XFDIMAGE_HPP
#define XFDIMAGE_HPP

/// Includes
#include "types.hpp"
#include "diskimage.hpp"
#include "imagestream.hpp"
///

/// Forwards
class Machine;
///

/// Class XFDImage
// This class implements XFD images, simple byte-for-byte-copies of
// an Atari disk.
class XFDImage : public DiskImage {
  //
  // If opened from a file, here it is.
  class ImageStream *Image;
  //
  // Protection Status of the image. True if this is write
  // protected.
  bool               Protected;
  //
  // Sector size of the image in bytes, and as upshift (exponent
  // to the power of two).
  UWORD              SectorSz;
  UBYTE              SectorShift;
  //
  // Size of the image in bytes.
  ULONG              ByteSize;
  //
public:
  XFDImage(class Machine *mach);
  virtual ~XFDImage(void);
  //
  // Open a disk image from a file given an image stream.
  virtual void OpenImage(class ImageStream *image);
  //
  // Reset the image to the start of it.
  virtual void Reset(void);
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector);  
  // Return the number of sectors.
  virtual ULONG SectorCount(void);
  //
  // Return the status of this image.
  virtual UBYTE Status(void);
  //
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer,UWORD &);
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD sector,const UBYTE *buffer,UWORD &);
  //  
  // Protect an image on user request
  virtual void ProtectImage(void);
};
///

///
#endif
