/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: casstream.cpp,v 1.4 2013/06/01 15:12:24 thor Exp $
 **
 ** In this module: Disk image class for .cas (tape) archives.
 **********************************************************************************/

/// Includes
#include "casstream.hpp"
#include "casfile.hpp"
#include "exceptions.hpp"
#include "stdio.hpp"
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
    class CASFile cas(File);
    do {
      cas.Get();
      size++;
    } while(true);
  } catch (int) {
    // Just the EOF.
  }
  //
  // Round up to the next multiple of 128 to allow this to be loaded
  // as an XFD image.
  Size   = (size + 127) & (~127);
  // Now allocate the buffer.
  Buffer = new UBYTE[Size];
  memset(Buffer,0,Size);
  //
  // Now start over.
  if (fseek(File,0,SEEK_SET) < 0) {
    ThrowIo("CASStream::OpenImage","unable to rewind the CAS input file");
  }
  //
  try {
    class CASFile cas(File);
    UBYTE *target = Buffer;
    do {
      *target++ = cas.Get();
    } while(target < Buffer + Size);
  } catch(int) {
    // EOF
  } 
  //
  // Replicate the last data as if it was loaded from the
  // tape buffer.
  if ((size & 127) && (size > 128)) {
    memcpy(Buffer + size,Buffer + size - 128,Size - size);
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
