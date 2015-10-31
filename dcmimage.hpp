/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: dcmimage.hpp,v 1.6 2015/07/14 20:01:00 thor Exp $
 **
 ** In this module: Disk image interface towards the dcm format
 **********************************************************************************/

#ifndef DCMIMAGE_HPP
#define DCMIMAGE_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "diskimage.hpp"
#include "imagestream.hpp"
///

/// Class DCMImage
// This class implements the image interface for DCM "compressed"
// disk images.
class DCMImage : public DiskImage {
  //
  // Image stream we get data from.
  class ImageStream *Image;
  //  
  // Sector size of the image in bytes, and as upshift (exponent
  // to the power of two) for all but the first three sectors
  // which are fixed to 128 bytes.
  UWORD              SectorSz;
  UBYTE              SectorShift;

  // The size of the image, decompressed
  ULONG              Size;
  //
  // The image as byte image.
  UBYTE             *Contents;
  //
  //
  // Input buffer for reading
  UBYTE             *IoBuffer;
  UBYTE             *IoPtr;
  UBYTE             *IoEnd;
  // Offset within the buffer.
  ULONG              IoOffset;
  ULONG              IoSize;
  static const ULONG IoBufferSize INIT(256);
  // Helper methods: Read a single byte buffered from the stream,
  // fail on EOF.
  UBYTE GetC(void);
  //
  // Some internal "uncompression" routines. Oh well, all this looks
  // pretty trivial. The best DCM can do is a simple RLE encoding, and
  // not even a very good one.
  /// DCMStream::DecodeModifyBegin
  //
  // Decode DCM "compression" mechanism "ModifyBegin"
  // Alters only the beginning of the sector, not the
  // tail.
  void DecodeModifyBegin(UBYTE *lastsector);
  //
  // Decode DCM "dos" sector by just placing data at bytes
  // 125,126 and 127. This is definitely a no-no for 256 byte sectored
  // images. We make a tempting approach to fix this somewhat....
  void DecodeDosSector(UBYTE *lastsector);
  //
  // Decode a DCM RLE "compressed" sector by reading
  // repeat/data runs and filling them into the sector
  void DecodeCompressed(UBYTE *lastsector);
  //
  // "Decompress" the sector by just altering the end of the previous
  // sector.
  void DecodeModifyEnd(UBYTE *lastsector);
  //
  // Copy a sector uncompressed from the source to the target.
  void DecodeUncompressed(UBYTE *lastsector);
  //
  //
public:
  DCMImage(class Machine *mach);
  virtual ~DCMImage(void);
  //
  // Open the image from a file name.
  virtual void OpenImage(class ImageStream *stream);
  //
  // Reset the image after turning the drive off and on again.
  virtual void Reset(void);
  //  
  // Return the sector size given the sector offset passed in.
  virtual UWORD SectorSize(UWORD sector);  
  //
  // Return the number of sectors.
  virtual ULONG SectorCount(void);
  //
  // Get the status of the disk image.
  virtual UBYTE Status(void)
  {
    // Since we cannot write back
    // to the compressed image without recompression, we currently 
    // return true here: always protected
    return DiskImage::Protected;
  }
  //  
  // Read a sector from the image into the supplied buffer. The buffer size
  // must fit the above SectorSize. Returns the SIO status indicator.
  virtual UBYTE ReadSector(UWORD sector,UBYTE *buffer,UWORD &delay);
  //
  // Write a sector to the image from the supplied buffer. The buffer size
  // must fit the sector size above. Returns also the SIO status indicator.
  virtual UBYTE WriteSector(UWORD sector,const UBYTE *buffer,UWORD &delay);
  //
  // Protect an image on user request
  virtual void ProtectImage(void);  
  //
};
///

///
#endif
