/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atrimage.cpp,v 1.14 2015/07/14 20:01:00 thor Exp $
 **
 ** In this module: Disk image class for .atr images.
 **********************************************************************************/

/// Includes
#include "atrimage.hpp"
#include "exceptions.hpp"
#include "imagestream.hpp"
#include "machine.hpp"
///

/// Statics
#ifndef HAS_MEMBER_INIT
const UBYTE   ATRImage::ATR_Header::Magic1 = 0x96;
const UBYTE   ATRImage::ATR_Header::Magic2 = 0x02;  
#endif
///

/// ATRImage::ATRImage
ATRImage::ATRImage(class Machine *mach)
  : DiskImage(mach),
    Image(NULL), Protected(false),
    SectorSz(128), SectorShift(7),
    BrokenDDImage(false)
{
}
///

/// ATRImage::~ATRImage
ATRImage::~ATRImage(void)
{
}
///

/// ATRImage::Reset
// Reset the image after turning the drive off and on again.
void ATRImage::Reset(void)
{
}
///

/// ATRImage::OpenImage
// Open a disk image from a file. This requires to read
// the sector size and other nifties.
void ATRImage::OpenImage(class ImageStream *image)
{
  ULONG numsecs,expected,ns;
  struct ATR_Header header;
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (Image) {
    Throw(ObjectExists,"ATRImage::OpenImage",
	  "the image has been opened already");
  }
#endif
  //
  ByteSize  = image->ByteSize();
  Protected = image->ProtectionStatus();
  //
  // Check whether the size minus the header size is divisible by 128. 
  // If not,then this is not an ATR disk image.
  if (ByteSize < sizeof(header) || ((ByteSize - sizeof(header)) & 0x7f)) {
    Throw(InvalidParameter,"ATRImage::OpenImage",
	  "file is not an atr image file");
  }
  // Ok, forget about the header size now.
  ByteSize -= sizeof(header);
  //
  // Read the header node now. This must also work or we are wrong here.
  if (image->Read(0,&header,sizeof(header)) == false) {
    ThrowIo("ATRImage::OpenImage","unable to read the ATR header");
  }
  //
  // Check whether the size of the header makes any sense and fits with the
  // file size. In case of doubt, use the file size.
  numsecs =   ULONG(header.seccountlo) | (ULONG(header.seccounthi) << 8) | 
    (ULONG(header.hiseccountlo) << 16) | (ULONG(header.hiseccounthi) << 24);
  //
  // Bugger! This doesn't count sectors, but 8th of sectors. Oh well...
  if (numsecs & 0x07) {
    Throw(InvalidParameter,"ATRImage::OpenImage","sector count of ATR image is invalid");
  }
  //
  // Fill in the sector size in bytes now. This must be either 128 or 256.
  // We might support other sector sizes (2048 for CD, 512 for HD) later. 
  SectorSz = UWORD(header.secsizelo | (header.secsizehi << 8));
  if (SectorSz != 128 && SectorSz != 256 && SectorSz != 512) {
    Throw(InvalidParameter,"ATRImage::OpenImage","sector size of ATR image is invalid");
  }
  ns = numsecs >> 4;
  //
  // Now compute the file size we expect and compare with the real file size.
  if (SectorSz == 256 && numsecs > 3*8) {
    // Special case 256 byte sectors: The first three are 128 bytes large.
    numsecs     = (numsecs - 3*8) / 16 + 3;
    expected    = (numsecs - 3) * 256 + 3 * 128;
    SectorShift = 8;
  } else if (SectorSz == 512) {
    numsecs   >>= 5;
    expected    = numsecs * 512;
    SectorShift = 9;
  } else {
    numsecs   >>= 3;
    expected    = numsecs * 128;
    SectorShift = 7;
  }
  SectorCnt     = numsecs;
  BrokenDDImage = false;
  if (expected != ByteSize) {
    // Harddisk image.
    if (ByteSize == (ns << 8)) {
      // Hard disk image?
      SectorCnt     = ns;
      SectorShift   = 8;
      BrokenDDImage = true;
    } else {
      // Print a warning message now.
      Machine->PutWarning("ATR header mangled. Trying to fix it....\n");
      //
      // Ok, now try to fix. Find some frequent byte sizes.
      if (ByteSize == 128*1040) {
	// An ED disk, most likely.
	SectorCnt     = 1040;
	SectorShift   = 7;
      } else if (ByteSize == 128*720) {
	// An SD disk, most likely.
	SectorCnt     = 720;
	SectorShift   = 7;
      } else if (ByteSize == 256*720 - 3*128) {
	// A proper HD disk, most likely.
	SectorCnt     = 720;
	SectorShift   = 8;
      } else if (ByteSize == 256*720) {
	// An HD image that has invalid first three sectors.
	SectorCnt     = 720;
	SectorShift   = 8;
	BrokenDDImage = true;
      }
    }
  }  
  //
  Image     = image;
}
///

/// ATRImage::SectorSize
// Return the sector size of the image.
UWORD ATRImage::SectorSize(UWORD sector)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATRImage::SectorSize","image is not yet open");
#endif
  // The first three sectors are 128 bytes large, always.
  if (SectorSz == 256 && sector <= 3)
    return 128;
  // This is independent of the sector offset.
  return SectorSz;
}
///

/// ATRImage::SectorCount
// Return the number of sectors in this image here.
ULONG ATRImage::SectorCount(void)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATRImage::SectorCount","image is not yet open");
#endif
  return SectorCnt;
}
///

/// ATRImage::Status
// Return the status bits of the image.
UBYTE ATRImage::Status(void)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATRImage::ProtectionStatus","image is not yet open");
#endif
  if (Protected)
    return DiskImage::Protected;
  return 0;
}
///

/// ATRImage::ReadSector
// Read a sector from the image into the supplied buffer. The buffer size
// must fit the above SectorSize. Returns the SIO status indicator.
UBYTE ATRImage::ReadSector(UWORD sector,UBYTE *buffer,UWORD &)
{
  ULONG offset,size;
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATRImage::ReadSector","image is not yet open");
#endif
  if (sector == 0 || sector > SectorCnt)
    return 'E';
  // Compute the sector offset now.
 if (SectorSz == 512) {
    offset = ULONG(sector - 1) << SectorShift;
    size   = SectorSz;
 } else if (sector <= 3) {
    // The 128 byte-case.
    offset = ULONG(sector - 1) << 7;
    size   = 128;
  } else {
    offset = (ULONG(sector - 4) << SectorShift) + 3 * 128;
    size   = SectorSz;
  }  
  if (BrokenDDImage) {
    // Broken case. All sectors have the same size.
    offset = ULONG(sector - 1) << SectorShift;
  }   
  offset += sizeof(struct ATR_Header);
  //
  if (Image->Read(offset,buffer,size))
    return 'C'; // completed fine.
  return 'E';   // did not.
}
///

/// ATRImage::WriteSector
// Write a sector to the image from the supplied buffer. The buffer size
// must fit the sector size above. Returns also the SIO status indicator.
UBYTE ATRImage::WriteSector(UWORD sector,const UBYTE *buffer,UWORD &)
{
  ULONG size,offset;
  //
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"ATRImage::WriteSector","image is not yet open");
#endif 
  if (sector == 0 || sector > SectorCnt)
    return 'E';
  // Compute the sector offset now.
  if (SectorSz == 512) {
    offset = ULONG(sector - 1) << SectorShift;
    size   = SectorSz;
  } else if (sector <= 3) {
    // The 128 byte-case.
    offset = ULONG(sector - 1) << 7;
    size   = 128;
  } else {
    offset = (ULONG(sector - 4) << SectorShift) + 3 * 128;
    size   = SectorSz;
  }   
  if (BrokenDDImage) {
    // Broken case. All sectors have the same size.
    offset = ULONG(sector - 1) << SectorShift;
  }   
  offset += sizeof(struct ATR_Header);
  //
  // If the image is protected, do not perform the write.
  if (Protected)
    return 'E';
  // Leave it to the image to perform the reading.
  if (Image->Write(offset,buffer,size))
    return 'C'; // completed fine.
  return 'E';   // did not.
}
///

/// ATRImage::ProtectImage
// Protect this image.
void ATRImage::ProtectImage(void)
{
  Protected = true;
}
///

/// ATRImage::FormatDisk
// Specific for ATR images (that form the default image class):
// Build a new ATR image of the given characteristics: sector size, and sector count
void ATRImage::FormatDisk(class ImageStream *target,UWORD sectorsize,ULONG sectorcount)
{
  struct ATR_Header header;
  ULONG offset,cnt,sector;
  UBYTE buffer[512];
#if CHECK_LEVEL > 0
  if (target == NULL)
    Throw(ObjectDoesntExist,"ATRImage::FormatDisk","no image given");
  if (sectorsize != 128 && sectorsize != 256 && sectorsize != 512)
    Throw(InvalidParameter,"ATRImage::FormatDisk","sector size invalid");
  if (sectorcount == 0)
    Throw(OutOfRange,"ATRImage::FormatDisk","invalid number of sectors");
#endif
  // 
  // Initialize the header file now. This is the size of the file in 
  // 16 byte blocks excluding the header.
  if (sectorsize == 256) {
    cnt = ((sectorcount - 3) * sectorsize + 128 * 3) >> 4;
  } else {
    cnt = (sectorsize * sectorcount) >> 4;
  }
  memset(&header,0,sizeof(header));
  header.magic1        = ATR_Header::Magic1;
  header.magic2        = ATR_Header::Magic2;
  header.seccountlo    = UBYTE(cnt);
  header.seccounthi    = UBYTE(cnt >> 8);
  header.secsizelo     = UBYTE(sectorsize);
  header.secsizehi     = UBYTE(sectorsize  >> 8);
  header.hiseccountlo  = UBYTE(cnt >> 16);
  header.hiseccounthi  = UBYTE(cnt >> 24);
  offset               = sizeof(header);
  memset(buffer,0,sizeof(buffer));
  if (!target->Write(0,&header,offset))
    ThrowIo("ATRImage::FormatDisk","unable to write ATR header of image file");
  //
  // Now clean all sectors.
  sector = 1; // sectors count from one up.
  do {
    if (sectorsize == 256 && sector <= 3) {
      // The first three sectors are 128 bytes large
      if (!target->Write(offset,buffer,128))
	ThrowIo("ATRImage::FormatDisk","unable to clean an image sector");
      offset += 128;
    } else {
      if (!target->Write(offset,buffer,sectorsize))
	ThrowIo("ATRImage::FormatDisk","unable to clean an image sector");
      offset += sectorsize;
    }
    sector++;
  } while(--sectorcount);
}
///
