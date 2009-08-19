/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: casstream.cpp,v 1.2 2009-08-10 14:42:17 thor Exp $
 **
 ** In this module: Disk image class for .cas (tape) archives.
 **********************************************************************************/

/// Includes
#include "casstream.hpp"
#include "exceptions.hpp"
#include "stdio.hpp"
///

/// CASStream::CASFile::Get
// Read a byte from the image, throw on EOF.
UBYTE CASStream::CASFile::Get(void)
{
  // Check whether there is still data in the buffer.
  do {
    if (inbuf < bytecnt) {
      return buffer[inbuf++];
    } else {
      UBYTE chunk[8];
      ULONG size;
      // Otherwise, read the next chunk from the CAS file.
      if (fread(chunk,1,8,src) != 8) {
	if (errno)
	  ThrowIo("CASStream::CASFile::Get","error when reading from CAS file");
	else
	  Throw(OutOfRange,"CASStream::CASFile::Get","unexpected EOF when reading from CAS file");
      }
      //
      // Get the size of this chunk.
      size = chunk[4] | (chunk[5] << 8);
      //
      // We only care about data chunks here.
      if (chunk[0] == 'd' && chunk[1] == 'a' && chunk[2] == 't' && chunk[3] == 'a') {
	UBYTE i,chksum = 0;
	if (size != sizeof(buffer))
	  Throw(InvalidParameter,"CASStream::CASFile::Get","CAS buffer segment size invalid");
	if (fread(buffer,1,size,src) != size) {
	  if (errno)
	    ThrowIo("CASStream::CASFile::Get","error when reading from CAS file");
	  else
	    Throw(OutOfRange,"CASStream::CASFile::Get","unexpected EOF when reading from CAS file");
	}	
	// Initialize the input buffer pointer and the size of the buffer.
	inbuf = 3;
	// Skip the buffer header.
	if (buffer[0] != 0x55 && buffer[1] != 0x55)
	  Throw(InvalidParameter,"CASStream::CASFile::Get","invalid CAS chunk, sync marker missing");
	switch(buffer[2]) {
	case 0xfc: // A full chunk
	  bytecnt = 128 + 3;
	  break;
	case 0xfa: // A partial chunk.
	  bytecnt = buffer[3+127] + 3;
	  if (bytecnt >= 128 + 3)
	    Throw(InvalidParameter,"CASStream::CASFile::Get","invalid CAS length indicator");
	  break;
	case 0xfe: // An EOF chunk.
	  bytecnt = 0;
	  break;
	default:
	  Throw(InvalidParameter,"CASStream::CASFile::Get","invalid CAS chunk type");
	}
	// Compute the checksum.
	for(i = 0,chksum = 0;i < size - 1;i++) {
	  if (buffer[i] + int(chksum) >= 256) {
	    chksum += buffer[i] + 1; // also add the carry;
	  } else {
	    chksum += buffer[i];
	  }
	}
	if (chksum != buffer[size - 1])
	  Throw(InvalidParameter,"CASStream::CASFile::Get","CAS chunk checksum is invalid");
	//
	// Everything is now initialized. Just throw on EOF now.
	if (buffer[2] == 0xfe)
	  throw 0;
      } else {
	// Skip the data.
	while(size) {
	  if (fgetc(src) < 0) {
	    if (errno)
	      ThrowIo("CASStream::CASFile::Get","error when reading from CAS file");
	    else
	      Throw(OutOfRange,"CASStream::CASFile::Get","unexpected EOF when reading from CAS file");
	  }
	  size--;
	}
      }
    }
  } while(true);
}
///

/// CASStream::CASStream
CASStream::CASStream(void)
  : File(NULL), Buffer(NULL), Size(0)
{
}
///

/// CASStream::~CASStream
CASStream::~CASStream(void)
{
  if (File)
    fclose(File);

  delete[] Buffer;
}
///

/// CASStream::OpenImage
void CASStream::OpenImage(const char *filename)
{
  ULONG size;
  
#if CHECK_LEVEL > 0
  if (File) {
    Throw(ObjectExists,"FileStream::OpenImage",
          "the image has been opened already");
  }
#endif
  File = fopen(filename,"rb");
  if (File == NULL)
    ThrowIo("FileStream::OpenImage","unable to open the input stream");
  //
  // First pass: Compute the size of the archive.
  size = 0;
  try {
    struct CASFile cas(File);
    do {
      cas.Get();
      size++;
    } while(true);
  } catch (int) {
    // Just the EOF.
  }
  //
  // Now allocate the buffer.
  Buffer = new UBYTE[size];
  Size   = size;
  //
  // Now start over.
  if (fseek(File,0,SEEK_SET) < 0) {
    ThrowIo("CASStream::OpenImage","unable to rewind the CAS input file");
  }
  //
  try {
    struct CASFile cas(File);
    UBYTE *target = Buffer;
    do {
      *target++ = cas.Get();
    } while(target < Buffer + Size);
  } catch(int) {
    // EOF
  } 
}
///

/// CASStream::Read
// Reads data from the image into the buffer from the given
// byte offset. Returns a boolean success indicator, does not
// throw.
bool CASStream::Read(ULONG offset,void *buffer,ULONG size)
{
#if CHECK_LEVEL > 0
  if (File == NULL)
    Throw(ObjectDoesntExist,"CASStream::Read","the image has not yet been opened");
#endif
  // Try to read past the end?
  if (offset + size > Size)
    return false;
  //
  // Otherwise, just copy the data out.
  memcpy(buffer,Buffer + offset,size);
  return true;
}
///
