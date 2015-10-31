/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atrimage.hpp,v 1.8 2015/07/14 20:01:00 thor Exp $
 **
 ** In this module: Disk image class for .atr images.
 **********************************************************************************/

#ifndef ATRIMAGE_HPP
#define ATRIMAGE_HPP

/// Includes
#include "types.hpp"
#include "diskimage.hpp"
#include "imagestream.hpp"
///

/// Forwards
class Machine;
///

/// Class ATRImage
// This class implements ATR images, disk images with a header node
// defining the type of the image.
class ATRImage : public DiskImage {
  //
  // If opened from a file, here it is.
  class ImageStream *Image;
  //
  // Protection Status of the image. True if this is write
  // protected.
  bool               Protected;
  //
  // Sector size of the image in bytes, and as upshift (exponent
  // to the power of two) for all but the first three sectors
  // which are fixed to 128 bytes.
  UWORD              SectorSz;
  UBYTE              SectorShift;
  //
  // Size of the image in bytes.
  ULONG              ByteSize;
  //
  // Set if the first three sectors have the wrong size on DD disks
  // (256 instead of 128)
  bool               BrokenDDImage;
  //
  // Number of sectors in this image.
  ULONG              SectorCnt;
  // 
  // The following header is used for ATR disk images
  struct ATR_Header {
    static const UBYTE   Magic1 INIT(0x96);
    static const UBYTE   Magic2 INIT(0x02);
    UBYTE                magic1; // must be Magic1 for correct ATR
    UBYTE                magic2; // must be Magic2 for correct ATR
    // FIX: The "seccount" below is not the number of sectors.
    // It is rather the number of sixteen-byte-arrangements. Wierd.
    // This number must be divided by eight to get the real number
    // of sectors (or multiplied by sixteen to get the byte size).
    UBYTE                seccountlo;
    UBYTE                seccounthi;
    UBYTE                secsizelo;
    UBYTE                secsizehi;
    UBYTE                hiseccountlo;
    UBYTE                hiseccounthi;
    UBYTE                gash[8];
  };
  //
public:
  ATRImage(class Machine *mach);
  virtual ~ATRImage(void);
  //
  // Open a disk image from a file given an image stream.
  virtual void OpenImage(class ImageStream *image);
  //
  // Reset to the initial stage.
  virtual void Reset(void);
  //
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector);  
  // Return the number of sectors.
  virtual ULONG SectorCount(void);
  //
  // Return the disk status.
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
  //
  // Specific for ATR images (that form the default image class):
  // Build a new ATR image of the given characteristics: sector size, and sector count
  static void FormatDisk(class ImageStream *target,UWORD sectorsize,ULONG sectorcount);
};
///

///
#endif
