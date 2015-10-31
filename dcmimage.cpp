/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: dcmimage.cpp,v 1.9 2015/07/14 20:01:00 thor Exp $
 **
 ** In this module: Disk image interface towards the dcm format
 **********************************************************************************/

/// Includes
#include "filestream.hpp"
#include "exceptions.hpp"
#include "dcmimage.hpp"
#include "string.hpp"
#include "new.hpp"
///

/// Statics
#ifndef HAS_MEMBER_INIT
const ULONG DCMImage::IoBufferSize = 256;
#endif
///

/// DCMImage::DCMImage
DCMImage::DCMImage(class Machine *mach)
  : DiskImage(mach), Image(NULL),
    Size(0), Contents(NULL),
    IoBuffer(NULL), IoPtr(NULL), IoEnd(NULL), IoOffset(0)
{ }
///

/// DCMImage::~DCMImage
// Cleanup the file stream now, close the file, dispose
// all buffers.
DCMImage::~DCMImage(void)
{
  delete[] Contents;
  delete[] IoBuffer;
}
///

/// DCMImage::GetC
// Helper methods: Read a single byte buffered from the stream,
// fail on EOF.
UBYTE DCMImage::GetC(void)
{
  ULONG readsize;
#if CHECK_LEVEL > 0
  if (IoBuffer == NULL || IoPtr == NULL)
    Throw(ObjectDoesntExist,"DCMImage::GetC","Io input buffer has not been build");
#endif
  //
  // Check whether we reached the end of the buffer. If not, read from the buffer
  // and return immediately.
  if (IoPtr < IoEnd) {
    return *IoPtr++;
  } else {
    // Check whether we already reached the end of the image. If so, then throw.
    if (IoOffset >= IoSize) {
      Throw(InvalidParameter,"DCMImage::Get","DCM input file mangled, premature EOF");
    }
    //
    // We need to refill the pointer. Use the image class to read data from there.
    readsize = IoSize - IoOffset;
    if (readsize > IoBufferSize)
      readsize = IoBufferSize;
    if (!Image->Read(IoOffset,IoBuffer,readsize))
      ThrowIo("DCMImage::Get","DCM image cannot read from input stream");
    //
    // Otherwise, re-seat the buffer pointer and continue.
    IoPtr     = IoBuffer;
    IoEnd     = IoBuffer + readsize;
    IoOffset += readsize;
    return *IoPtr++;
  }
}
///

/// DCMImage::DecodeModifyBegin
// Decode DCM "compression" mechanism "ModifyBegin"
// Alters only the beginning of the sector, not the
// tail.
void DCMImage::DecodeModifyBegin(UBYTE *lastsector)
{
  UWORD offset;

  offset = GetC();

  //
  if (offset >= SectorSz)
    Throw(OutOfRange,"DCMImage::DecodeModifyBegin","DCM byte offset is out of range");
  // And now modify the beginning of the sector with something else, but
  // backwards...
  // Note that offset == 0 means that we have to modify a single byte.
  lastsector += offset;
  do {
    *lastsector = GetC();
    lastsector--;
  } while(offset--);
}
///

/// DCMImage::DecodeDosSector
// Decode DCM "dos" sector by just placing data at bytes
// 125,126 and 127. This is definitely a no-no for 256 byte sectored
// images. We make a tempting approach to fix this somewhat....
void DCMImage::DecodeDosSector(UBYTE *lastsector)
{
  UBYTE data;

  data = GetC();
  memset(lastsector,data,SectorSz - 3);

  lastsector   += SectorSz - 3;
  // Why should I define the last four bytes? DOS 2.0S only uses the last
  // three bytes! Oh well, but it's really like that...
  *lastsector++ = GetC();
  *lastsector++ = GetC();
  *lastsector++ = GetC();
  *lastsector++ = GetC();
}
///

/// DCMImage::DecodeCompressed
// Decode a DCM RLE "compressed" sector by reading
// repeat/data runs and filling them into the sector
void DCMImage::DecodeCompressed(UBYTE *lastsector)
{
  UBYTE data;
  UWORD end;
  UWORD offset = 0;

  do {
    end = GetC();
    if (offset && end == 0) {
      // First offset is literal. All following offsets have to translate 0->256
      end = 256;
    }
    //
    // Check for proper sizes now.
    if (end > SectorSz || end < offset)
      Throw(OutOfRange,"DCMImage::DecodeCompressed","DCM data run end offset is out of range");
    //
    // Get a run of data. This run might be zero bytes large.
    while(offset < end) {
      *lastsector++ = GetC();
      offset++;
    }
    // We're done if we reached the end of the sector.
    if (offset >= SectorSz)
      break;
    //
    // Get now the end offset for the RLE encoder.
    end  = GetC();
    if (end == 0) end = 256; // This always translates.
    data = GetC();
    // Check again for the run length
    if (end > SectorSz || end < offset) 
      Throw(OutOfRange,"DCMImage::DecodeCompressed","DCM data run end offset is out of range");
    //
    // Insert a run of similar data.
    while(offset < end) {
      *lastsector++ = data;
      offset++;
    }
  } while(offset < SectorSz);
}
///

/// DCMImage::DecodeModifyEnd
// "Decompress" the sector by just altering the end of the previous
// sector.
void DCMImage::DecodeModifyEnd(UBYTE *lastsector)
{	
  UBYTE offset;

  offset = GetC();

  if (offset > SectorSz)
    Throw(OutOfRange,"DCMImage::DecodModifyEnd","found out of range offset in DCM stream");

  lastsector += offset;
  while(offset < SectorSz) {
    *lastsector++ = GetC();
    offset++;
  }
}
///

/// DCMImage::DecodeUncompressed
// Copy a sector uncompressed from the source to the target.
void DCMImage::DecodeUncompressed(UBYTE *lastsector)
{
  UWORD offset = SectorSz;

  do {
    *lastsector++ = GetC();
  } while(--offset);
}
///

/// DCMImage::Reset
// Reset the image after turning the drive off and on again.
void DCMImage::Reset(void)
{
}
///

/// DCMImage::OpenImage
// Open a disk image from an image stream. Fill in
// the protection status and other nifties, decompress
// the image.
void DCMImage::OpenImage(class ImageStream *image)
{
  UWORD sector,nextsector,numsectors;
  UBYTE *target;
  UBYTE in,pass,type;
  UBYTE lastsector[256];
  bool  lastpass;
  //
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (IoBuffer || Contents || Image) {
    Throw(ObjectExists,"DCMImage::OpenImage",
	  "the image has been opened already");
  }
#endif
  //
  // Insert the contents, allocate IoBuffers and stuff like that.
  Image       = image;
  IoBuffer    = new UBYTE[IoBufferSize];
  IoPtr       = IoBuffer;
  IoEnd       = IoBuffer;
  IoSize      = Image->ByteSize();
  //
  //    
  // Reset statistics.
  sector     = 1;
  pass       = 1;
  lastpass   = false;
  // Shut up the compiler with some settings.
  target     = NULL;
  numsectors = 720;
  type       = 0; // reset archive type
  //
  do {
    bool abort;
    //
    // Check whether we are just filling up because we reached the end of the
    // archive. If not, read the header.
    if (lastpass) {
      // Ok, just for fill-up. Ignore anything else, and fill up to the last
      // sector with blanks.
      nextsector = UWORD(numsectors + 1);
      abort      = true;
    } else {
      // Read the header. We only support single-file archives of type 0xfa.
      if (GetC() != 0xfa)
	Throw(InvalidParameter,"DCMImage::OpenImage","unsupported or invalid DCM archive format");
      //
      // Get the archive type, and the pass flag, and the pass counter.
      in     = GetC();
      // For the first pass: Get parameters.
      if (pass == 1) {
	// Get the archive type and keep it for latter passes.
	type = UBYTE(in & 0x70);
	switch(type) {
	case 0x00:
	  // Single density.
	  numsectors  = 720;
	  SectorSz    = 128;
	  SectorShift = 7;
	  break;
	case 0x20:
	  // Double density.
	  numsectors  = 720;
	  SectorSz    = 256;
	  SectorShift = 8;
	  break;
	case 0x40:
	  // Enhanced density
	  numsectors  = 1040;
	  SectorSz    = 128;
	  SectorShift = 7;
	  break;
	default:
	  // invalid density.
	  Throw(InvalidParameter,"DCMImage::OpenStream","invalid DCM density specification");
	  break;
	}
	// We do not need any kind of pass number or continuation flag, but we check for consistency, at least.
	if ((pass ^ in) & 0x1f)
	  Throw(PhaseError,"DCMImage::OpenStream","unexpected pass sequence counter");
	//
	// Compute the total size of the output: One complete disk.
	// Ok, we could use smarter memory allocations and linked lists and
	// all these nifties for this, but I don't want to. First of all, 170KB
	// is not really much memory these days, and second of all, splitting it
	// into fragments doesn't help either. I need to keep it all in memory
	// somehow somewhere.
	Size     = ULONG(numsectors) * SectorSz; 
	Contents = new UBYTE[Size];
	//
	// We must blank out the disk contents here. This is the
	// equivalent of formatting the disk.
	memset(Contents,0,Size);
	target   = Contents;
      } else {
	// We just check wether the archive type or the pass is consistent.
	if ((type ^ in) & 0x70)
	  Throw(PhaseError,"DCMImage::OpenStream","inconsistent density in latter DCM pass");
	// And check the pass information as well.
	if ((pass ^ in) & 0x1f)
	  Throw(PhaseError,"DCMImage::OpenStream","unexpected pass sequence counter");      
      }
      //      
      // Check whether this pass is the last one.
      lastpass    = (in & 0x80)?(true):(false);
      //
      // Get the first sector in this pass. Skip all sectors before, i.e. clear them to zero.
      nextsector  = GetC();
      nextsector |= (UWORD(GetC()) << 8);
      abort       = false;
    }
    //
    // Now generate zero-sectors up to the point where we continue with the decoding.
    // For that, advance the fill-in target by the sector size.
    if (sector < nextsector) {
      target  += (nextsector - sector) << SectorShift;
      sector   = nextsector;
    }
    while(!abort) {
      bool expectsector;
      //
      in           = GetC();
      // Check whether we expect a sector number behind the decoding type or not.
      // This happens only if bit 7 is false.
      expectsector = (in & 0x80)?(false):(true);
      // Unless noted otherwise, the next sector to decode is the current sector plus one.
      nextsector   = sector + UWORD(1);
      // Now switch by the compression type of this sector.
      switch(in & 0x7f) {
      case 0x41:
	DecodeModifyBegin(lastsector);
	break;
      case 0x42:
	DecodeDosSector(lastsector);
	break;
      case 0x43:
	DecodeCompressed(lastsector);
	break;
      case 0x44:
	DecodeModifyEnd(lastsector);
	break;
      case 0x45:
	// This is the end of the pass. Continue with the next pass, or not.
	abort        = true;
	break;
      case 0x46:
	// Identical sector, do not touch sector buffer contents.
	break;
      case 0x47:
	DecodeUncompressed(lastsector);
	break;
      default:
	Throw(InvalidParameter,"DCMImage::OpenStream","invalid DCM compression scheme");
	break;
      }
      // Abort now if this pass is done, advance to the next pass then, unless this is
      // the last pass anyhow. In this case, fill up the remaining disk up to the last
      // sector with zeros.
      if (abort) {
	pass++;
	break;
      }
      //
      // Install the data for this sector, and advance the sector count.
      memcpy(target,lastsector,SectorSz);
      target += SectorSz;
      sector++;
      // Now check whether we should expect another sector number from here.
      if (expectsector) {
	// Get the next sector number we should touch. Otherwise, the next sector
	// is just the current sector plus one, as initialized
	nextsector  = GetC();
	nextsector |= (UWORD(GetC()) << 8);
	// Special DCM bug workaround. If this is signaled to be the last pass
	// and the next sector indication is 0x45, Then this was, in fact, just an
	// "abort pass" indicator that got corrupted into the next sector. We perform
	// the similar action as above.
	if (lastpass && nextsector == 0x45)
	  break;
	if (nextsector < sector) {
	  Throw(InvalidParameter,"DCMStream::OpenStream","invalid next sector specification");
	}
      }
      //
      // Now advance the sector number until we reach the next non-blank sector.
      if (sector < nextsector) {
	target += (nextsector - sector) << SectorShift;
	sector  = nextsector;
      }
    }
  } while(sector <= numsectors);
  //
  // Well, that's about it.
}
///

/// DCMImage::Read
// Reads data from the image into the buffer from the given
// byte offset.
UBYTE DCMImage::ReadSector(UWORD sector,UBYTE *buffer,UWORD &)
{
  ULONG offset,size;
#if CHECK_LEVEL > 0
  if (Contents == NULL)
    Throw(ObjectDoesntExist,"DCMImage::ReadSector","image is not yet open");
#endif
  if (sector == 0)
    return 'E';
  // Compute the sector offset now. This is easy as DCM all stores
  // in identically sized sectors, including the first three.
  offset   = ULONG(sector - 1) << SectorShift;
  if (sector <= 3) {
    // The 128 byte-case.
    size   = 128;
  } else {
    size   = SectorSz;
  }
  if (offset > Size)
    return 'E';
  //
  memcpy(buffer,Contents + offset,size);
  return 'C'; // completed fine.
}
///

/// DCMImage::WriteSector
// Write a sector to the image from the supplied buffer. The buffer size
// must fit the sector size above. Returns also the SIO status indicator.
// As we cannot write into the compressed image, return always an error
// here.
UBYTE DCMImage::WriteSector(UWORD,const UBYTE *,UWORD &)
{
  return 'E';
}
///

/// DCMImage::ProtectImage
// Protect an image on user request. Does nothing
// as this image is always protected.
void DCMImage::ProtectImage(void)
{
}
///

/// DCMImage::SectorSize
// Return the sector size of the image.
UWORD DCMImage::SectorSize(UWORD sector)
{
#if CHECK_LEVEL > 0
  if (Contents == NULL)
    Throw(ObjectDoesntExist,"DCMImage::SectorSize","image is not yet open");
#endif
  // The first three sectors are 128 bytes large, always.
  if (sector <= 3)
    return 128;
  // This is independent of the sector offset.
  return SectorSz;
}
///

/// DCMImage::SectorCount
// Return the number of sectors in this image here.
ULONG DCMImage::SectorCount(void)
{
#if CHECK_LEVEL > 0
  if (Contents == NULL)
    Throw(ObjectDoesntExist,"DCMImage::SectorCount","image is not yet open");
#endif
  return Size >> SectorShift;
}
///
