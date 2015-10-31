/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: zstream.cpp,v 1.5 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: Disk image source stream interface towards stdio
 **********************************************************************************/

/// Includes
#include "filestream.hpp"
#include "exceptions.hpp"
#include "zstream.hpp"
#include "new.hpp"
#if HAVE_ZLIB_H && HAVE_GZOPEN
#include <zlib.h>
///

/// ZStream::ZStream
ZStream::ZStream(void)
  : InFile(NULL), Size(0)
{ }
///

/// ZStream::~ZStream
// Cleanup the file stream now, close the file.
ZStream::~ZStream(void)
{
  struct Buffer *buf;
  // Dispose all buffers now.
  while((buf = Contents.RemHead())) {
    delete buf;
  }
  //
  if (InFile) {
    gzclose(InFile);
  }
}
///

/// ZStream::OpenImage
// Open a disk image from a file name. Fill in
// the protection status and other nifties.
void ZStream::OpenImage(const char *name)
{
  struct Buffer *buf;
  ULONG  offset;
  //
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (InFile) {
    Throw(ObjectExists,"FileStream::OpenImage",
	  "the image has been opened already");
  }
#endif
  //
  InFile        = gzopen(name,"rb");
  if (InFile == NULL)
    ThrowIo("ZStream::OpenImage","unable to open the input stream");
  //
  // Now read all buffers into this stream here.
  offset = 0;
  do {
    UBYTE *dt;
    ULONG remains;
    //
    buf = new struct Buffer;
    Contents.AddTail(buf);
    buf->Offset = offset;
    //
    // Fill this buffer by the ZLib
    dt = buf->Data,remains = 256;
    do {
      int rd;
      //
      // Now read data from the zlib into this buffer.
      rd = gzread(InFile,dt,remains);
      // Abort on EOF: If that happens, we're done with reading the image
      if (rd == 0) {
	Size = offset;
	return;
      }
      if (rd < 0)
	ThrowIo("ZStream::OpenImage","failed to read from the Z image file");
      //
      // Increment data over the position.
      dt      += rd;
      remains -= rd;
      offset  += rd;
    } while(remains);
  } while(true);
}
///

/// ZStream::Read
// Reads data from the image into the buffer from the given
// byte offset.
bool ZStream::Read(ULONG offset,void *buffer,ULONG size)
{
  UBYTE *out;
  struct Buffer *buf;
  //
#if CHECK_LEVEL > 0
  if (InFile == NULL)
    Throw(ObjectDoesntExist,"ZStream::Read","the image has not yet been opened");
#endif
  // Try to read past the end?
  if (offset + size > Size)
    return false;
  //
  // Ok, things seem sane here. Find the buffer that is responsible for the
  // given offset. Since atari disk images are relatively small, we can simply seek backward
  // until we find it.
  buf = Contents.Last();
  while(buf && buf->Offset > offset) {
    buf = buf->PrevOf();
  }
  // If we are here, we either reached the end of the list (how that? Didn't we check
  // the offset?) or found a buffer, whose last byte is beyond what we need.
  out = (UBYTE *)buffer;
  while(size) {
    ULONG bufof;
    ULONG remain;
    //
#if CHECK_LEVEL > 0
    if (buf == NULL)
      Throw(PhaseError,"ZStream::Read","found premature EOF in the Z output stream");
#endif
    // Compute the offset within the buffer.
    bufof  = offset - buf->Offset;
    remain = 256 - bufof; // remaining bytes in the buffer.
    if (remain > size)
      remain = size;
    memcpy(out,buf->Data + bufof,remain);
    //
    // Advance to the next buffer and decrement the size
    out    += remain;
    size   -= remain;
    offset += remain;
    buf     = buf->NextOf();
  }
  return true;
}
///

/// ZStream::Write
// Write data back into the image at the given offset.
// We cannot write to a compressed stream, hence return an
// error condition here.
bool ZStream::Write(ULONG,const void *,ULONG)
{
  return false;
}
///

///
#endif
