/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: binaryimage.cpp,v 1.11 2005-09-10 12:55:39 thor Exp $
 **
 ** In this module: Disk image class for binary load files
 **********************************************************************************/

/// Includes
#include "binaryimage.hpp"
#include "exceptions.hpp"
#include "imagestream.hpp"
#include "choicerequester.hpp"
#include "machine.hpp"
#include "string.hpp"
///

/// BinaryImage::BootImage
// Disk bootblock for booting off a binary load file.
#ifndef LO_IMAGE
const UBYTE BinaryImage::BootImage[] = {
  0x00,  // bootflag
  0x03,  // # of sectors to boot
  0x00,  // lo boot address
  0x07,  // hi boot address
  0x99,  // lo run address
  0x07,  // hi run address ***
  // bootstrap code runs here
  0x20,0x07,0x08,                     // disk init
  0xa9,0x00,
  0x8d,0xe0,0x02,0x8d,0xe1,0x02,      // clear run vector
  0x8d,0xe2,0x02,0x8d,0xe3,0x02,      // clear init vector
  0x8d,0x7d,0x09,                     // set sector, hi
  0x8d,0x7f,0x09,0x85,0x49,           // init sector buffer to point to sector #4
  0xa9,0x04,0x8d,0x7e,0x09,           // set sector, lo
  //
  0x20,0xe8,0x07,                     // get byte
  0xc9,0xff,                          // must be 0xff for binary load
  0xd0,0x05,                          // branch on error
  0x20,0xe8,0x07,                     // get byte
  0xc9,0xff,                          // another 0xff must follow here
  0xd0,0x62,                          // branch on error to SEC:RTS
  //
  0x20,0xe8,0x07,                     // get start address, lo
  0x85,0x43,0x8d,0xe0,0x02,
  0x20,0xe8,0x07,                     // get start address, hi
  0x85,0x44,0x8d,0xe1,0x02,
  0x20,0xe8,0x07,                     // get end address, lo
  0x85,0x45,
  0x20,0xe8,0x07,                     // get end address, hi
  0x85,0x46,
  // loading loop starts here.
  0x20,0xe8,0x07,                     // get next byte
  0xa0,0x00,                          // reset Y
  0x91,0x43,                          // store result
  0xe6,0x43,                          // increment low
  0xd0,0x02,                          // carry over into high?
  0xe6,0x44,                          // if so, do it now.
  0xa5,0x45,0xc5,0x43,                // compare low-addresses
  0xa5,0x46,0xe5,0x44,                // compare high-addresses.
  0xb0,0xe9,                          // as LONG as this is greater or equal, repeat
  0xad,0xe2,0x02,0x0d,0xe3,0x02,      // check whether the init address has been set
  0xf0,0x0f,                          // if not, avoid the init method
  // run thru init vector
  0xa5,0x49,0x48,                     // keep sector offset
  0x20,0xfb,0x07,                     // jump into the init vector
  // for compatibility, wait for a vblank
  0x20,0x1f,0x08,0x20,0x1f,0x08,
  0x68,0x85,0x49,                     // restore sector offset
  // init vector done
  0x20,0xd4,0x07,                     // get next byte
  0xf0,0x0f,                          // on EOF, exit from boot process
  0x85,0x43,                          // next start, lo
  0x20,0xe8,0x07,                     // next start, hi
  0x85,0x44,
  0x25,0x43,0xc9,0xff,                // is this 0xff,0xff?
  0xf0,0xee,                          // if so, repeat
  0xd0,0xb4,                          // jump back to the loading part
  0xf0,0x09,                          // boot process end
  // here: exit on error: BOOT ERROR
  0x68,0x68,0x68,0x68,                // drop off return address
  0x38,0x60,        
  // here: run/init jumps
  0x6c,0xe2,0x02,                     // jump on init
  0x6c,0xe0,0x02,                     // jump on start
  // here: load new sector.
  0xad,0x7e,0x09,
  0x8d,0x0a,0x03,                     // set sector lo
  0xad,0x7d,0x09,                     // get sector hi (no need to mask, only one file)
  0x8d,0x0b,0x03,                     // install it
  0x0d,0x0a,0x03,0xf0,0x26,           // return with Z flag set on EOF.
  0xa9,0x31,0x8d,0x00,0x03,           // set disk drive
  0xa9,0x01,0x8d,0x01,0x03,           // set unit
  0xa9,0x52,0x8d,0x02,0x03,           // set CMD=read
  0xa9,0x00,0x8d,0x04,0x03,0x85,0x47, // set buffer address, lo
  0x85,0x49,                          // clear offset within sector
  0xa9,0x09,0x8d,0x05,0x03,0x85,0x48, // set buffer address, hi: page 9
  0x20,0x53,0xe4,                     // call SIO
  0x30,0xbf,                          // jump to boot error generation on error.
  0xa0,0x01,                          // clear Z flag
  0x60,                               //
  // here: get byte, return eq on EOF.
  0xa4,0x49,0xcc,0x7f,0x09,           // compare against the length of the sector
  0x90,0x08,                          // skip over if not end.
  0x20,0x9c,0x07,                     // reload sector
  0xd0,0x01,0x60,                     // skip return on not EOF
  0xa0,0x00,                          // reset Y
  0xb1,0x47,                          // read value
  0xe6,0x49,                          // read next value: increment, clear Z flag
  0x60,
  // here: get byte, error on EOF
  0xa4,0x49,0xcc,0x7f,0x09,           // compare against the length of the sector
  0x90,0x07,                          // skip over if not end.
  0x20,0x9c,0x07,                     // reload sector
  0xf0,0x9e,                          // branch to error on EOF.
  0xa0,0x00,                          // reset Y
  0xb1,0x47,                          // read value
  0xe6,0x49,                          // read next value: increment, clear Z flag
  0x60,
  //
  0x20,0x96,0x07,                     // call the init vector
  0xa9,0x00,                          // clear
  0x8d,0xe2,0x02,0x8d,0xe3,0x02,      // the init vector
  0x60,
  //
  0xa9,0x01,0x85,0x09,0xa9,0x00,
  0x8d,0x44,0x02,
  0xa9,0x77,0x85,0x0a,0x85,0x0c,
  0xa9,0xe4,0x85,0x0b,0x85,0x0d,
  0x4c,0x50,0xe4,                     // disk init
  // 
  // VBI waiter
  0xad,0x0b,0xd4,0xc9,0x70,0x90,0xf9,
  0xad,0x0b,0xd4,0xc9,0x20,0xb0,0xf9,
  0x60
};
#else
const UBYTE BinaryImage::BootImage[] = {
  0x00,  // bootflag
  0x03,  // # of sectors to boot
  0x00,  // lo boot address
  0x05,  // hi boot address
  0x99,  // lo run address
  0x05,  // hi run address ***
  // bootstrap code runs here
  0x20,0x50,0xe4,                     // disk init
  0xa9,0x00,
  0x8d,0xe0,0x02,0x8d,0xe1,0x02,      // clear run vector
  0x8d,0xe2,0x02,0x8d,0xe3,0x02,      // clear init vector
  0x8d,0x7d,0x09,                     // set sector, hi
  0x8d,0x7f,0x09,0x85,0x49,           // init sector buffer to point to sector #4
  0xa9,0x04,0x8d,0x7e,0x09,           // set sector, lo
  //
  0x20,0xe8,0x05,                     // get byte
  0xc9,0xff,                          // must be 0xff for binary load
  0xd0,0x05,                          // branch on error
  0x20,0xe8,0x05,                     // get byte
  0xc9,0xff,                          // another 0xff must follow here
  0xd0,0x62,                          // branch on error to SEC:RTS
  //
  0x20,0xe8,0x05,                     // get start address, lo
  0x85,0x43,0x8d,0xe0,0x02,
  0x20,0xe8,0x05,                     // get start address, hi
  0x85,0x44,0x8d,0xe1,0x02,
  0x20,0xe8,0x05,                     // get end address, lo
  0x85,0x45,
  0x20,0xe8,0x05,                     // get end address, hi
  0x85,0x46,
  // loading loop starts here.
  0x20,0xe8,0x05,                     // get next byte
  0xa0,0x00,                          // reset Y
  0x91,0x43,                          // store result
  0xe6,0x43,                          // increment low
  0xd0,0x02,                          // carry over into high?
  0xe6,0x44,                          // if so, do it now.
  0xa5,0x45,0xc5,0x43,                // compare low-addresses
  0xa5,0x46,0xe5,0x44,                // compare high-addresses.
  0xb0,0xe9,                          // as LONG as this is greater or equal, repeat
  0xad,0xe2,0x02,0x0d,0xe3,0x02,      // check whether the init address has been set
  0xf0,0x0f,                          // if not, avoid the init method
  // run thru init vector
  0xa5,0x49,0x48,                     // keep sector offset
  0x20,0xfb,0x05,                     // jump into the init vector
  // for compatibility, wait for a vblank
  0xa5,0x14,0xc5,0x14,0xf0,0xfc,
  0x68,0x85,0x49,                     // restore sector offset
  // init vector done
  0x20,0xd4,0x05,                     // get next byte
  0xf0,0x0f,                          // on EOF, exit from boot process
  0x85,0x43,                          // next start, lo
  0x20,0xe8,0x05,                     // next start, hi
  0x85,0x44,
  0x25,0x43,0xc9,0xff,                // is this 0xff,0xff?
  0xf0,0xee,                          // if so, repeat
  0xd0,0xb4,                          // jump back to the loading part
  0xf0,0x77,                          // boot process end
  // here: exit on error: BOOT ERROR
  0x68,0x68,0x68,0x68,                // drop off return address
  0x38,0x60,        
  // here: run/init jumps
  0x6c,0xe2,0x02,                     // jump on init
  0x6c,0xe0,0x02,                     // jump on start
  // here: load new sector.
  0xad,0x7e,0x09,
  0x8d,0x0a,0x03,                     // set sector lo
  0xad,0x7d,0x09,                     // get sector hi (no need to mask, only one file)
  0x8d,0x0b,0x03,                     // install it
  0x0d,0x0a,0x03,0xf0,0x26,           // return with Z flag set on EOF.
  0xa9,0x31,0x8d,0x00,0x03,           // set disk drive
  0xa9,0x01,0x8d,0x01,0x03,           // set unit
  0xa9,0x52,0x8d,0x02,0x03,           // set CMD=read
  0xa9,0x00,0x8d,0x04,0x03,0x85,0x47, // set buffer address, lo
  0x85,0x49,                          // clear offset within sector
  0xa9,0x09,0x8d,0x05,0x03,0x85,0x48, // set buffer address, hi: page 9
  0x20,0x53,0xe4,                     // call SIO
  0x30,0xbf,                          // jump to boot error generation on error.
  0xa0,0x01,                          // clear Z flag
  0x60,                               //
  // here: get byte, return eq on EOF.
  0xa4,0x49,0xcc,0x7f,0x09,           // compare against the length of the sector
  0x90,0x08,                          // skip over if not end.
  0x20,0x9c,0x05,                     // reload sector
  0xd0,0x01,0x60,                     // skip return on not EOF
  0xa0,0x00,                          // reset Y
  0xb1,0x47,                          // read value
  0xe6,0x49,                          // read next value: increment, clear Z flag
  0x60,
  // here: get byte, error on EOF
  0xa4,0x49,0xcc,0x7f,0x09,           // compare against the length of the sector
  0x90,0x07,                          // skip over if not end.
  0x20,0x9c,0x05,                     // reload sector
  0xf0,0x9e,                          // branch to error on EOF.
  0xa0,0x00,                          // reset Y
  0xb1,0x47,                          // read value
  0xe6,0x49,                          // read next value: increment, clear Z flag
  0x60,
  //
  0x20,0x96,0x05,                     // call the init vector
  0xa9,0x00,                          // clear
  0x8d,0xe2,0x02,0x8d,0xe3,0x02,      // the init vector
  0x60,
  //
  0xa9,0x01,0x85,0x09,0xa9,0x00,
  0x8d,0x44,0x02,
  0xa9,0x77,0x85,0x0a,0x85,0x0c,
  0xa9,0xe4,0x85,0x0b,0x85,0x0d,
  0x6c,0xe0,0x02
};
#endif
///

/// BinaryImage::BinaryImage
BinaryImage::BinaryImage(class Machine *mach)
  : DiskImage(mach), Contents(NULL), Image(NULL), 
    FixupRequester(new ChoiceRequester(mach))
{
}
///

/// BinaryImage::~BinaryImage
BinaryImage::~BinaryImage(void)
{
  delete[] Contents;
  delete FixupRequester;
}
///

/// BinaryImage::OpenImage
// Open a disk image from a file. This requires to read
// the sector size and other nifties.
void BinaryImage::OpenImage(class ImageStream *image)
{
  ULONG filesize,imagesectors,offset;
  UBYTE *dest;
  UBYTE *file;
  UWORD nextsector;
  bool firstsector = true;
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (Image || Contents) {
    Throw(ObjectExists,"BinaryImage::OpenImage",
	  "the image has been opened already");
  }
#endif
  //
  filesize     = image->ByteSize();
  imagesectors = (filesize + 124) / 125;
  //
  // Ok, we know now how much we have to allocate for the disk:
  // The size of the binary, rounded to sectors, plus the
  // size of the header, also rounded to sectors.
  ByteSize  = ((sizeof(BootImage) + 0x7f) & (~0x7f)) + (imagesectors << 7);
  Contents  = new UBYTE[ByteSize];
  // Quiet down valgrind and initialize the sectors to zero to make them
  // initialized.
  memset(Contents,0,ByteSize);
  // Copy the file into the buffer now.
  memcpy(Contents,BootImage,sizeof(BootImage));
  //
  // Read the image, and add the Dos 2.0S type sector linkage into bytes 125..127.
  dest       = Contents + ((sizeof(BootImage) + 0x7f) & (~0x7f));
  file       = dest;
  offset     = 0;
  nextsector = 1 + (((sizeof(BootImage) + 0x7f)) >> 7); // sectors count from one.
  while(filesize) {
    ULONG databytes = filesize;
    // 125 data bytes per sectors.
    if (databytes > 125) {
      databytes  = 125;
      nextsector++;
    } else {
      // This is the last sector in the file, hence no further linkage
      nextsector = 0;
    }
    //
    if (!image->Read(offset,dest,databytes)) {
      Throw(InvalidParameter,"BinaryImage::OpenImage","could not read binary load file");
    }
    // Check whether this is a hacked-broken image file we must
    // repair. This is not nice, but what to do?
    if (firstsector) {
      firstsector = false;
      if (databytes == 125) {
	// Loader is from 0x400 to 0x466
	if (dest[2] == 0x00 && dest[3] == 0x04 &&
	    dest[4] == 0x66 && dest[5] == 0x04 &&
	    dest[6] == 0xa9 && dest[7] == 0x1f) {
	  // Could be, but need not to be...
	  if (dest[0x22] == 0xea &&
	      dest[0x23] == 0xea &&
	      dest[0x24] == 0xea) {
	    if (FixupRequester->Request("Detected hacked broken binary loader, shall I try to fix it?",
					"Fix it!","Leave alone!",NULL) == 0) {
	      //
	      // Ok, replace the NOPs by the sector increment that has been removed
	      dest[0x23] = 0xee;
	      dest[0x24] = 0x6b;
	      dest[0x25] = 0x04;
	    }
	  }
	}
      }
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
  }
  VerifyImage(file);
  Image = image;
}
///

/// BinaryImage::FilePointer::FilePointer
// Initialize a DOS 2.0S style file pointer.
BinaryImage::FilePointer::FilePointer(UBYTE *src)
  : Sector(src), ByteOffset(0)
{ }
///

/// BinaryImage::FilePointer::Get
// Read from the file pointer the next byte, similar to what Dos 2.0S would
// have done.
UBYTE BinaryImage::FilePointer::Get(void)
{
  for(;;) {
    if (ByteOffset < Sector[127]) {
      // Still bytes in the sector. Get the byte and read it.
      return Sector[ByteOffset++];
  }
    // We must advance to the "next sector". Luckely, this is
    // just linearly arranged.
    if ((Sector[125] | Sector[126]) == 0) {
      // Next sector would be zero. Bummer! An EOF.
      throw -1;
    }
    // Skip over the sector linkage.
    Sector    += 128;
    ByteOffset = 0;
  }
}
///

/// BinaryImage::FilePointer::Put
// Write the given data to the stream, do not extend and
// do not write on EOF.
void BinaryImage::FilePointer::Put(UBYTE data)
{
  for(;;) {
    if (ByteOffset < Sector[127]) {
      // Still bytes in the sector. Get the byte and read it.
      Sector[ByteOffset++] = data;
      return;
    }
    // We must advance to the "next sector". Luckely, this is
    // just linearly arranged.
    if ((Sector[125] | Sector[126]) == 0) {
      // Next sector would be zero. Bummer! An EOF.
      throw -1;
    }
    // Skip over the sector linkage.
    Sector    += 128;
    ByteOffset = 0;
  }
}
///

/// BinaryImage::FilePointer::GetWord
// Read a word from a binary load file, first lo, then hi.
UWORD BinaryImage::FilePointer::GetWord(void)
{
  UWORD lo,hi;
  //
  // Get lo, hi in the required order.
  lo = Get();
  hi = Get();
  //
  // Return them.
  return lo | (hi << 8);
}
///

/// BinaryImage::FilePointer::PutWord
// Read a word from a binary load file, first lo, then hi.
void BinaryImage::FilePointer::PutWord(UWORD data)
{
  // Write lo first,then hi.
  Put(data & 0xff);
  Put(data >> 8);
}
///

/// BinaryImage::FilePointer::Eof
// Check whether the next read will cause an EOF condition.
bool BinaryImage::FilePointer::Eof(void) const
{
  if (ByteOffset >= Sector[127] && Sector[125] == 0 && Sector[126] == 0)
    return true;
  return false;
}
///

/// BinaryImage::FilePointer::Truncate
// Truncate the binary load file at the indicated file position
void BinaryImage::FilePointer::Truncate(void)
{
  Sector[127] = ByteOffset;
  Sector[126] = 0;
  Sector[125] = 0;
}
///

/// BinaryImage::VerifyImage
// Check whether the loaded image is setup fine.
void BinaryImage::VerifyImage(UBYTE *src)
{
  struct FilePointer file(src);
  struct FilePointer backup = file;
  struct FilePointer adr = file;
  UWORD start,end,data;
  //
  // Now perform the check.
  try {
    // We must have an FF,FF header.
    data  = 0, start = 0xffff;
    start = file.GetWord();
    if (start != 0xffff)
      throw 1;
    //
    // Get low and hi address, start.
    start = file.GetWord();
    adr   = file; // keep position of end address
    end   = file.GetWord();
    //
    do {
      // Check whether they are sorted correctly.
      if (start > end)
	throw 0;
      //
      // Get all data from the file, skipping over it.
      do {
	file.Get();
	data++;
	start++;
      } while(start <= end);
      //
      // Data phase worked fine.
      data = 0;
      // If this is now an EOF, everything is fine.
      if (file.Eof())
	return;
      // Up to here the image was fine. Make a backup.
      backup = file;
      // Get the next start address. Might be 0xff 0xff.
      do {
	start = file.GetWord();
      } while (start == 0xffff);
      adr = file; // keep position of end address
      end = file.GetWord();
    } while(true);
  } catch(int error) {
    // Here something wicked happened. We'll see...
    switch(error) {
    case 0:
      if (FixupRequester->Request("Binary load structure seems damaged, start address > end address.\n"
				  "Shall I try to fix it?","Fix it!","Leave alone",NULL) == 0) {
	backup.Truncate();
      }
      break;
    case 1:
      Machine->PutWarning("Binary load file header is missing, this file will most likey not work.");
      break;
    case -1:
      if (FixupRequester->Request("Binary load structure seems damaged, detected unexpected end of file.\n"
				  "Shall I try to fix it?","Fix it!","Leave alone",NULL) == 0) {	
	// Can we fix it by providing a better end address?
	if (data) {
	  adr.PutWord(start-1);
	} else {
	  backup.Truncate();
	}
      }
      break;
    }
  }
}
///

/// BinaryImage::SectorSize
// Return the sector size of the image.
UWORD BinaryImage::SectorSize(UWORD)
{
#if CHECK_LEVEL > 0
  if (Image == NULL || Contents == NULL)
    Throw(ObjectDoesntExist,"BinaryImage::SectorSize","image is not yet open");
#endif
  // This emulated always 128 byte sectors.
  return 128;
}
///

/// BinaryImage::SectorCount
// Return the number of sectors of this image
ULONG BinaryImage::SectorCount(void)
{
#if CHECK_LEVEL > 0
  if (Image == NULL || Contents == NULL)
    Throw(ObjectDoesntExist,"BinaryImage::SectorSize","image is not yet open");
#endif
  //
  // We only emulate 128 byte sectors here.
  return ByteSize >> 7;
}
///

/// BinaryImage::ProtectionStatus
// Return the protection status of the image.
bool BinaryImage::ProtectionStatus(void)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"BinaryImage::ProtectionStatus","image is not yet open");
#endif
  // This is always write-protected as we have no means to re-construct the binary from
  // the disk image.
  return true;
}
///

/// BinaryImage::ReadSector
// Read a sector from the image into the supplied buffer. The buffer size
// must fit the above SectorSize. Returns the SIO status indicator.
UBYTE BinaryImage::ReadSector(UWORD sector,UBYTE *buffer)
{
  ULONG offset;
#if CHECK_LEVEL > 0
  if (Image == NULL || Contents == NULL)
    Throw(ObjectDoesntExist,"BinaryImage::ReadSector","image is not yet open");
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

/// BinaryImage::WriteSector
// Write a sector to the image from the supplied buffer. The buffer size
// must fit the sector size above. Returns also the SIO status indicator.
UBYTE BinaryImage::WriteSector(UWORD,const UBYTE *)
{
  // We cannot write to these files. Just return an error.
  return 'E';
}
///

/// BinaryImage::ProtectImage
// Protect this image. As binary images are always write-protected,
// nothing happens here.
void BinaryImage::ProtectImage(void)
{
}
///
