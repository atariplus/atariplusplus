/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: binaryimage.cpp,v 1.20 2015/11/07 18:53:12 thor Exp $
 **
 ** In this module: Disk image class for binary load files
 **********************************************************************************/

/// Includes
#include "binaryimage.hpp"
#include "exceptions.hpp"
#include "imagestream.hpp"
#include "choicerequester.hpp"
#include "machine.hpp"
#include "adrspace.hpp"
#include "cpu.hpp"
#include "string.hpp"
///

/// BinaryImage::BinaryImage
BinaryImage::BinaryImage(class Machine *mach)
  : DiskImage(mach), PatchProvider(mach), Patch(mach,this,1), 
    Contents(NULL), Image(NULL), BootStage(0),
    FixupRequester(new ChoiceRequester(mach))
{
  PatchProvider::InstallPatchList();
}
///

/// BinaryImage::~BinaryImage
BinaryImage::~BinaryImage(void)
{
  PatchProvider::Remove();
  delete[] Contents;
  delete FixupRequester;
}
///

/// BinaryImage::Reset
void BinaryImage::Reset(void)
{
  BootStage   = 0;
  LoaderStage = 0;
  PatchProvider::InstallPatchList();

  if (Contents)
    CreateBootSector(Contents);
}
///

/// BinaryImage::CreateBootSector
// Create the boot sector with the binary image
void BinaryImage::CreateBootSector(UBYTE *bootimage)
{
  if (bootimage) {
    bootimage[0]  = 0; // boot flag
    bootimage[1]  = 1; // number of sectors
    bootimage[2]  = 0; // load address, lo
    bootimage[3]  = 7; // load address, hi
    bootimage[4]  = 0x77; // run address, lo
    bootimage[5]  = 0xe4; // run address, hi
    bootimage[6]  = ESC_Code; 
    bootimage[7]  = loaderEscape;
    bootimage[8]  = 0x38; // sec
    bootimage[9]  = 0x60; // rts
    bootimage[10] = 0x6c; // at address 0x70a: jmp init
    bootimage[11] = 0xe2;
    bootimage[12] = 0x02;
    bootimage[13] = 0x6c; // at address 0x70d: jmp run
    bootimage[14] = 0xe0;
    bootimage[15] = 0x02;
  }
}
///

/// BinaryImage::CreateVTOC
// Create the VTOC at offset 0x168
void BinaryImage::CreateVTOC(UBYTE *image,UWORD totalcount)
{
  image[0x05] = 0x00; // VTOC is valid
  image[0x00] = 0x02; // type is Dos 2.0S
  image[0x01] = UBYTE(totalcount & 0xff); // total number of sectors, low
  image[0x02] = UBYTE(totalcount >> 8  ); // total number of sectors, high
}
///

/// BinaryImage::CreateDirectory
// Create the directory at offset 0x169
void BinaryImage::CreateDirectory(UBYTE *image,UWORD sectorcount)
{
  // Create the direcgtory entry
  image[0x00] = 0x62; // locked, in-use
  image[0x01] = UBYTE(sectorcount & 0xff);
  image[0x02] = UBYTE(sectorcount >> 8  );
  image[0x03] = 4; // start sector, low
  image[0x04] = 0; // start sector, hi
  memcpy(image + 0x05,"AUTORUN SYS",8+3);
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
  UWORD totalcount;
  bool firstsector = true;
  //
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
  if (imagesectors + 3 > 0xffff)
    Throw(OutOfRange,"BinaryImage::OpenImage",
	  "image file too large, must fit into 65533 sectors");
  totalcount   = UWORD(imagesectors + 3);
  //
  // initialize the state machine.
  BootStage     = 0;
  //
  // Ok, we know now how much we have to allocate for the disk:
  // The size of the binary, rounded to sectors, plus the
  // size of the header, also rounded to sectors.
  ByteSize  = 128 * 3 + (imagesectors << 7);
  // Also make sure that we have enough sectors for the VTOC at 0x168
  // and eight directory sectors.
  if (3 + imagesectors < 0x168) {
    ByteSize   = 0x170 * 128;
    totalcount = 0x170;
  } else {
    // Otherwise, add nine sectors for the VTOC and the directory.
    ByteSize   += 9 * 128;
    totalcount += 9;
  }
  Contents  = new UBYTE[ByteSize];
  // Quiet down valgrind and initialize the sectors to zero to make them
  // initialized.
  memset(Contents,0,ByteSize);
  //
  // Copy the file into the buffer now.
  CreateBootSector(Contents);
  // Create the VTOC at sector 0x168
  CreateVTOC(Contents + (0x168 - 1) * 128,totalcount);
  // Create the directory at sector 0x169
  CreateDirectory(Contents + (0x169 - 1) * 128,UWORD(imagesectors));
  //
  // Read the image, and add the Dos 2.0S type sector linkage into bytes 125..127.
  dest       = Contents + 128 * 3;
  file       = dest;
  offset     = 0;
  nextsector = 4; // sectors count from one.
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
	    if (FixupRequester->Request("Detected hacked broken binary loader, "
					"shall I try to fix it?",
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
    // If the next sector would link into the VTOC, skip that part.
    if (nextsector == 0x168) {
      nextsector = 0x171;
      dest[125]  = UBYTE(nextsector >> 8);
      dest[126]  = UBYTE(nextsector);
      dest[127]  = UBYTE(databytes);
      // Advance to the next sector (the VTOC) and skip
      // the nine following sectors.
      dest      += 128 * 10;
    } else {
      dest[125]  = UBYTE(nextsector >> 8);
      dest[126]  = UBYTE(nextsector);
      dest[127]  = UBYTE(databytes);
      dest      += 128;
    }
    //
    offset   += databytes;
    filesize -= databytes;
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
      if (FixupRequester->Request("Binary load structure seems damaged, "
				  "start address > end address.\n"
				  "Shall I try to fix it?",
				  "Fix it!","Leave alone",NULL) == 0) {
	backup.Truncate();
      }
      break;
    case 1:
      DiskImage::Machine->PutWarning("Binary load file header is missing, "
				     "this file will most likey not work.");
      break;
    case -1:
      if (FixupRequester->Request("Binary load structure seems damaged, "
				  "detected unexpected end of file.\n"
				  "Shall I try to fix it?",
				  "Fix it!","Leave alone",NULL) == 0) {	
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

/// BinaryImage::Status
// Return the protection status of the image.
UBYTE BinaryImage::Status(void)
{
#if CHECK_LEVEL > 0
  if (Image == NULL)
    Throw(ObjectDoesntExist,"BinaryImage::ProtectionStatus","image is not yet open");
#endif
  // This is always write-protected as we have no means to re-construct the binary from
  // the disk image.
  return DiskImage::Protected;
}
///

/// BinaryImage::ReadSector
// Read a sector from the image into the supplied buffer. The buffer size
// must fit the above SectorSize. Returns the SIO status indicator.
UBYTE BinaryImage::ReadSector(UWORD sector,UBYTE *buffer,UWORD &)
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
UBYTE BinaryImage::WriteSector(UWORD,const UBYTE *,UWORD &)
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

/// BinaryImage::RunPatch
// This entry is called by the CPU emulator to run the patch at hand
// whenever an ESC (HLT, JAM) code is detected.
void BinaryImage::RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE)
{
  UWORD sector;
  // If the current sector is #1, assume that the image just got loaded and
  // reset the state.
  sector  = adr->ReadByte(0x030a);
  sector |= adr->ReadByte(0x030b) << 8;
  if (sector == 1) {
    BootStage   = 0;
    LoaderStage = 0;
  }

  switch(BootStage) {
  case Init: // Initialize the stage machine
    InitStage(adr,cpu);
    break;
  case Fill: // Read the next sector into the buffer.
    FillStage(adr,cpu);
    break;
  case SIOReturn: // SIO returned. Check status.
    SIOReturnStage(adr,cpu);
    // No need to go through another CPU cycle if we proceed directly to the loader.
    if (BootStage != Loader)
      break;
    // otherwise, run into here.
  case Loader: // bytes are ready in the buffer, run the binary loader.
    do {
      if (NextByte < AvailBytes) {
	// There are bytes in the buffer. Read them and pass them on to the loader.
	RunLoaderStage(adr,cpu,adr->ReadByte(0x710 + NextByte++));
      } else {
	// No new bytes in the buffer. Load the next sector by returning to the
	// fill stage.
	FillStage(adr,cpu);
      }
    } while(BootStage == Loader);
    break;
  case JumpInit: // jump through the init vector if it is there.
    JumpInitStage(adr,cpu);
    if (BootStage != JumpRun)
      break;
  case JumpRun:
    JumpRunStage(adr,cpu);
    break;
  case JumpInitReturn:
    JumpInitReturnStage(adr,cpu);
    // runs into the following
  case WaitVBI:
    WaitVBIStage(adr,cpu);
    break;
  case WaitVBI2:
    WaitVBI2Stage(adr,cpu);
    break;
  }
}
///

/// BinaryImage::PushReturn
// Push the indicated return address onto the
// stack of the emulated CPU to make it call the
// given routine.
void BinaryImage::PushReturn(class AdrSpace *adr,class CPU *cpu,ADR where)
{
  ADR stackptr = 0x100 + cpu->S(); // stack pointer.
  // We need to push the return address -1 since the
  // RTS adds one to the return PC.
  where--;
  adr->WriteByte(stackptr--,where >> 8  ); // Hi goes first here.
  adr->WriteByte(stackptr--,where & 0xff); // Then Lo
  //
  // Now install the stack pointer back.
  cpu->S() = stackptr - 0x100;
}
///

/// BinaryImage::InitStage
// Initialize the boot process by setting a couple of Os variables.
void BinaryImage::InitStage(class AdrSpace *adr,class CPU *cpu)
{
  adr->WriteByte(0x09  ,0x01); // boot flag = 1: dos boot
  adr->WriteByte(0x0244,0x00); // no cold start
  adr->WriteByte(0x0a  ,0x77); // DOS run address: cold start
  adr->WriteByte(0x0b  ,0xe4);
  adr->WriteByte(0x0c  ,0xc0); // DOS init address: just an RTS
  adr->WriteByte(0x0d  ,0xe4);
  adr->WriteByte(0x02e0,0x00); // Reset the RUN address
  adr->WriteByte(0x02e1,0x00);
  adr->WriteByte(0x02e2,0x00); // Reset the INIT address
  adr->WriteByte(0x02e3,0x00);
  adr->WriteByte(0x030a,0x00); // Reset the sector.
  // Next sector to load
  NextSector  = 4;
  AvailBytes  = 0;
  PushReturn(adr,cpu,0x0706);  // call me again.
  PushReturn(adr,cpu,0xe450);  // call SIO init
  BootStage   = Fill;
  LoaderStage = CheckHeader;
}
///

/// BinaryImage::FillStage
// Read the next sector into the buffer
void BinaryImage::FillStage(class AdrSpace *adr,class CPU *cpu)
{
  if (NextSector) {
    adr->WriteByte(0x0300,0x31); // device to address: Disk
    adr->WriteByte(0x0301,0x01); // unit 1
    adr->WriteByte(0x0302,0x52); // read
    adr->WriteByte(0x0304,0x10); // buffer is at 0x710
    adr->WriteByte(0x0305,0x07);
    adr->WriteByte(0x030a,NextSector & 0x0ff);
    adr->WriteByte(0x030b,NextSector >> 8);
    PushReturn(adr,cpu,0x706);  // call me again.
    PushReturn(adr,cpu,0xe453); // call SIO
    BootStage = SIOReturn;
  } else {
    // An error. Generate a boot error.
    PushReturn(adr,cpu,0x0708);
    BootStage  = Init;
  }
}
///

/// BinaryImage::SIOReturnStage
// The call from SIO returned. Check the OS status and fill in the
// number of available bytes in this sector, and the next sector.
void BinaryImage::SIOReturnStage(class AdrSpace *adr,class CPU *cpu)
{
  if (cpu->Y() == 0x01) {
    // All went fine. Get the data for the buffer.
    NextSector  = adr->ReadByte(0x0710 + 0x7e);
    NextSector |= adr->ReadByte(0x0710 + 0x7d) << 8;
    AvailBytes  = adr->ReadByte(0x0710 + 0x7f);
    NextByte    = 0x00;
    BootStage   = Loader;
  } else {
    // An error. Generate a boot error.
    PushReturn(adr,cpu,0x0708);
    BootStage  = Init;
  }
}
///

/// BinaryImage::JumpRunStage
void BinaryImage::JumpRunStage(class AdrSpace *adr,class CPU *cpu)
{
  if (NextByte == AvailBytes && NextSector == 0) {
    ADR run = adr->ReadByte(0x2e0) | (adr->ReadByte(0x2e1) << 8);
    
    if (run != 0) {
      PushReturn(adr,cpu,0x0708); // generate a boot error.
      PushReturn(adr,cpu,0x070d); // jump through the run vector.
    } else {
      PushReturn(adr,cpu,0x0708); // generate a boot error.
    }
    BootStage = Init;
  } else {
    PushReturn(adr,cpu,0x0706); // continue.
    BootStage = Loader;
  }
}
///

/// BinaryImage::JumpInitStage
// Jump through the init stage if it is there.
void BinaryImage::JumpInitStage(class AdrSpace *adr,class CPU *cpu)
{
  ADR init = adr->ReadByte(0x2e2) | (adr->ReadByte(0x2e3) << 8);

  if (init != 0) {
    PushReturn(adr,cpu,0x0706); // continue.
    PushReturn(adr,cpu,0x070a); // 
    BootStage = JumpInitReturn;
  } else if (StartAddress < 0x300) {
    BootStage = WaitVBI;
    PushReturn(adr,cpu,0x0706); // continue.
  } else {
    BootStage = JumpRun;
  }
}
///

/// BinaryImage::JumpInitReturnStage
// Return from the init vector, reset it again and prepare for the next segment.
void BinaryImage::JumpInitReturnStage(class AdrSpace *adr,class CPU *)
{
  adr->WriteByte(0x2e2,0x00);
  adr->WriteByte(0x2e3,0x00); // reset the init vector.
  // Wait at least for one VBI
  BootStage   = WaitVBI;
}
///

/// BinaryImage::WaitVBIStage
// Wait until a VBI happens, phase 1
void BinaryImage::WaitVBIStage(class AdrSpace *adr,class CPU *cpu)
{
  UBYTE line = adr->ReadByte(0xd40b);

  if (line > 0x60 || line < 0x40) {
    // Wait at least for line 0x40
    PushReturn(adr,cpu,0x0706); // continue.
  } else {
    BootStage = WaitVBI2; // the second round
    PushReturn(adr,cpu,0x0706);
  }
}
///

/// BinaryImage::WaitVBI2Stage
// Wait until a VBI happens, phase 2
void BinaryImage::WaitVBI2Stage(class AdrSpace *adr,class CPU *cpu)
{
  UBYTE line = adr->ReadByte(0xd40b);

  if (line >= 0x40 || line < 0x20) {
    // Wait until we cross the line 0
    PushReturn(adr,cpu,0x706);
  } else {
    BootStage = JumpRun;
    PushReturn(adr,cpu,0x706);
  }
}
///

/// BinaryImage::RunLoaderStage
// Place a single byte into the RAM or advance the loader.
// The byte is already placed in the argument.
void BinaryImage::RunLoaderStage(class AdrSpace *adr,class CPU *cpu,UBYTE byte)
{
  switch(LoaderStage) {
  case CheckHeader:
    if (byte != 0xff) {
      // Generate a boot error.
      PushReturn(adr,cpu,0x0708);
      BootStage   = Init;
    } else {
      LoaderStage = CheckHeader2;
    }
    break;
  case CheckHeader2:
    if (byte != 0xff) {
      // Generate a boot error.
      PushReturn(adr,cpu,0x0708);
      BootStage   = Init;
    } else {
      LoaderStage = StartAdrLo;
    }
    break;
  case StartAdrLo:
    StartAddress  = byte;
    LoaderStage   = StartAdrHi;
    break;
  case StartAdrHi:
    StartAddress |= byte << 8;
    // If this is 0xffff, then this is just a binary header again, which will be skipped.
    if (StartAddress == 0xffff) {
      LoaderStage = StartAdrLo;
    } else {
      // Start address is valid. If the run address is still not set, use this as run address.
      if (adr->ReadByte(0x2e0) == 0 && adr->ReadByte(0x2e1) == 0) {
	adr->WriteByte(0x2e0,StartAddress & 0xff);
	adr->WriteByte(0x2e1,StartAddress >> 8);
      }
      LoaderStage = EndAdrLo;
    }
    break;
  case EndAdrLo:
    EndAddress    = byte;
    LoaderStage   = EndAdrHi;
    break;
  case EndAdrHi:
    EndAddress   |= byte << 8;
    if (EndAddress < StartAddress) {
      // Generate a boot error.
      PushReturn(adr,cpu,0x0708);
      BootStage   = Init;
    } else {
      LoaderStage = ReadByte;
    }
    break;
  case ReadByte:
    // Place the next byte into the start address, and increment it.
    adr->WriteByte(StartAddress,byte);
    StartAddress++;
    if (StartAddress > EndAddress) {
      // Here the loaded block just ended. Reset the loader to
      // the start address
      LoaderStage = StartAdrLo;
      // But run through the init address.
      BootStage   = JumpInit;
      PushReturn(adr,cpu,0x0706); // continue
    }
    break;
  }
}
///
