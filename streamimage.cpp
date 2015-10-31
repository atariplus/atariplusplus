/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: streamimage.cpp,v 1.5 2015/07/14 20:01:00 thor Exp $
 **
 ** In this module: Disk image class for any type of files that need to be put on disk
 **********************************************************************************/

/// Includes
#include "streamimage.hpp"
#include "exceptions.hpp"
#include "string.hpp"
///

/// StreamImage::StreamImage
StreamImage::StreamImage(class Machine *mach,const char *name)
  : DiskImage(mach), Contents(NULL), Image(NULL), ByteSize(0), Name(name)
{ }
///

/// StreamImage::~StreamImage
StreamImage::~StreamImage(void)
{
  delete[] Contents;
}
///

/// StreamImage::Reset
// Reset the image after turning it on and off.
void StreamImage::Reset(void)
{
}
///

/// StreamImage::OpenImage
// Open a disk image from a file. This requires to read
// the sector size and other nifties.
void StreamImage::OpenImage(class ImageStream *image)
{
  ULONG filesize,imagesectors,offset;
  UBYTE *dest;
  const char *name;
  UWORD nextsector;
  int i;
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (Image || Contents) {
    Throw(ObjectExists,"StreamImage::OpenImage","the image has been opened already");
  }
#endif
  //
  filesize     = image->ByteSize();
  imagesectors = (filesize + 124) / 125;
  //
  // Additional sectors required: Three boot sectors which are left blank.
  imagesectors += 3;
  //
  // If the number of sectors is below 0x169, we need to add at least the directory
  // sectors and enough "footage" to create them.
  if (imagesectors < 0x168) {
    imagesectors = 0x170;
  } else {
    imagesectors += 9; // directory plus VTOC
  }
  // Ok, we know now how much we have to allocate for the disk:
  // The size of the binary, rounded to sectors, plus the
  // size of the header, also rounded to sectors.
  ByteSize  = imagesectors << 7;
  Contents  = new UBYTE[ByteSize];
  // Quiet down valgrind and initialize the sectors to zero to make them
  // initialized.
  memset(Contents,0,ByteSize);
  //
  // Create the boot sector at the first sector
  dest = Contents;
  *dest++ = 0x00; // boot flag
  *dest++ = 0x01; // sector count
  *dest++ = 0x00; // low byte load address
  *dest++ = 0x07; // hi-byte load address
  *dest++ = 0x06; // low-byte run address
  *dest++ = 0x07; // hi-byte run address
  *dest++ = 0x38; // init: SEC (for boot error)
  *dest++ = 0x60; // RTS
  //
  // Create the VTOC
  dest = Contents + ((0x168 - 1) << 7);
  *dest++ = 0x02; // DOS 2.0S
  *dest++ = UBYTE(imagesectors - 9 - 3); // # of total sectors
  *dest++ = UBYTE((imagesectors - 9 - 3) >> 8);
  //
  // Create the directory.
  dest = Contents + ((0x169 - 1) << 7);
  *dest++ = 0x62; // locked, DOS 2
  *dest++ = UBYTE((filesize + 124) / 125); // file size
  *dest++ = UBYTE(((filesize + 124) / 125) >> 8);
  *dest++ = 0x03; // start sector
  *dest++ = 0x00; // start sector
  // Now create the name.
  i = 0;
  name = Name;
  while(*name && *name != '.' && i < 8) {
    *dest++ = *name++;
    i++;
  }
  while(i < 8) {
    *dest++ = ' ';
    i++;
  }
  if (*name == '.') {
    // And the annex.
    name++;
    i = 0;
    while(*name && i < 3) {
      *dest++ = *name++;
      i++;
    }
  } else while(i < 8+3) {
    *dest++ = ' ';
    i++;
  }
  //
  // Now fill in the actual file into sectors.
  dest       = Contents + ((3 - 1) << 7);
  nextsector = 3;
  offset     = 0;
  while(filesize) {
    ULONG databytes = filesize;
    // 125 data bytes per sectors.
    if (databytes > 125) {
      databytes  = 125;
      nextsector++;
      if (nextsector == 0x168) {
	// Skip the directory and the VTOC
	nextsector += 9;
      }
    } else {
      // This is the last sector in the file, hence no further linkage
      nextsector = 0;
    }
    //
    if (!image->Read(offset,dest,databytes)) {
      Throw(InvalidParameter,"StreamImage::OpenImage","could not read binary load file");
    }
    //
    // Add linkage to the next sector in dos 2.0S way.
    dest[125] = UBYTE(nextsector >> 8);
    dest[126] = UBYTE(nextsector);
    dest[127] = UBYTE(databytes);
    //
    offset   += databytes;
    filesize -= databytes;
    dest     += 128;
    if (nextsector == 0x171) {
      // skipped the directory?
      dest   += 9 << 7;
    }
  }
  Image = image;
}
///

/// StreamImage::ReadSector
// Read a sector from the image into the supplied buffer. The buffer size
// must fit the above SectorSize. Returns the SIO status indicator.
UBYTE StreamImage::ReadSector(UWORD sector,UBYTE *buffer,UWORD &)
{
  ULONG offset;
#if CHECK_LEVEL > 0
  if (Image == NULL || Contents == NULL)
    Throw(ObjectDoesntExist,"StreamImage::ReadSector","image is not yet open");
#endif
  if (sector == 0)
    return 'E';
  // Compute the offset from the sector.
  offset = (sector - 1)<<7;
  // Read past the end of the image?
  if (offset + 128 > ByteSize)
    return 'E';
  //
  // Otherwise, just copy the data over.
  memcpy(buffer,Contents+offset,128);
  return 'C'; // completed fine.
}
///
