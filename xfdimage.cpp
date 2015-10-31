/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xfdimage.cpp,v 1.7 2015/07/14 20:01:00 thor Exp $
 **
 ** In this module: Disk image class for .xfd images.
 **********************************************************************************/

/// Includes
#include "xfdimage.hpp"
#include "exceptions.hpp"
#include "imagestream.hpp"
#include "xfdimage.hpp"
///

/// XFDImage::XFDImage
XFDImage::XFDImage(class Machine *mach)
  : DiskImage(mach),
    Image(NULL), Protected(false),
    SectorSz(128), SectorShift(7)
{
}
///

/// XFDImage::~XFDImage
XFDImage::~XFDImage(void)
{
}
///

/// XFDImage::Reset
// Reset the image to the start of it.
void XFDImage::Reset(void)
{
}
///

/// XFDImage::OpenImage
// Open a disk image from a file. This requires to read
// the sector size and other nifties.
void XFDImage::OpenImage(class ImageStream *image)
{
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (Image) {
    Throw(ObjectExists,"XFDImage::OpenImage",
	  "the image has been opened already");
  }
#endif
  //
  ByteSize  = image->ByteSize();
  Protected = image->ProtectionStatus();
  //
  // Check whether the size is divisible by 128. If not,then
  // this is not a disk image.
  if (ByteSize & 0x7f) {
    Throw(InvalidParameter,"XFDImage::OpenImage",
	  "file is not an xfd image file");
  }
  // Guess the sector size by checking for some natural sizes.
  // If we have 720*256 bytes, we assume a DD sectored image,
  // otherwise it is ED or SD.
  if (ByteSize == 720*256) {
    SectorSz    = 256;
    SectorShift = 8;
  } else {
    SectorSz    = 128;
    SectorShift = 7;
  }
  //
  // We're done now, keep the image
  Image     = image;
}
///

/// XFDImage::SectorSize
// Return the sector size of the image.
UWORD XFDImage::SectorSize(UWORD sector)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"XFDImage::SectorSize","image is not yet open");
#endif
  // The first three sectors are 128 bytes large, always.
  if (sector <= 3)
    return 128;
  // This is independent of the sector offset.
  return SectorSz;
}
///

/// XFDImage::SectorCount
// Return the number of sectors in this image here.
ULONG XFDImage::SectorCount(void)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"XFDImage::SectorCount","image is not yet open");
#endif
  // All sectors have the same size here.
  return ByteSize >> SectorShift;
}
///

/// XFDImage::Status
// Return the status of the image.
UBYTE XFDImage::Status(void)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"XFDImage::ProtectionStatus","image is not yet open");
#endif
  if (Protected)
    return DiskImage::Protected;
  return 0;
}
///

/// XFDImage::ReadSector
// Read a sector from the image into the supplied buffer. The buffer size
// must fit the above SectorSize. Returns the SIO status indicator.
UBYTE XFDImage::ReadSector(UWORD sector,UBYTE *buffer,UWORD &)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"XFDImage::ReadSector","image is not yet open");
#endif
  if (sector == 0)
    return 'E';
  // Leave it to the image to perform the reading.
  if (Image->Read(ULONG(sector - 1)<<SectorShift,buffer,(sector > 3)?SectorSz:128))
    return 'C'; // completed fine.
  return 'E';   // did not.
}
///

/// XFDImage::WriteSector
// Write a sector to the image from the supplied buffer. The buffer size
// must fit the sector size above. Returns also the SIO status indicator.
UBYTE XFDImage::WriteSector(UWORD sector,const UBYTE *buffer,UWORD &)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"XFDImage::WriteSector","image is not yet open");
#endif
  // If the image is protected, do not perform the write.
  if (Protected || sector == 0)
    return 'E';
  // Leave it to the image to perform the reading.
  if (Image->Write(ULONG(sector - 1)<<SectorShift,buffer,(sector > 3)?SectorSz:128))
    return 'C'; // completed fine.
  return 'E';   // did not.
}
///

/// XFDImage::ProtectImage
// Protect this image.
void XFDImage::ProtectImage(void)
{
  Protected = true;
}
///
