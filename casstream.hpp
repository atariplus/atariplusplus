/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: casstream.hpp,v 1.3 2015/09/20 20:22:28 thor Exp $
 **
 ** In this module: Disk image class for .cas (tape) archives.
 **********************************************************************************/

#ifndef CASSTREAM_HPP
#define CASSTREAM_HPP

/// Includes
#include "types.hpp"
#include "imagestream.hpp"
#include "stdio.hpp"
///

/// Forwards
class Machine;
///

/// Class CASStream
// This class defines an disk image for tape archives. We
// prepend to this image a binary-load mini-dos to boot off
// the image without fuzz, and a dummy directory that allows
// regular dos'es to load the tape image.
class CASStream : public ImageStream {
  //
  // The machine this belongs to. This is used for
  // error reporting.
  class Machine *machine;
  //
  // The file pointer by which this is open.
  FILE          *File;
  //
  // The buffer keeping the decoded archive.
  UBYTE         *Buffer;
  //
  // The size of the buffer.
  ULONG          Size;
  //
public:
  CASStream(class Machine *mach);
  virtual ~CASStream(void);
  //
  // Open an image given the file name.
  virtual void OpenImage(const char *filename);
  //
  // Clean an image stream = truncate it to zero bytes.
  virtual bool FormatImage(const char *)
  {
    return false; // not supported
  }
  //
  // Get the size of an image in bytes.
  virtual ULONG ByteSize(void)
  {
    return Size;
  }
  //
  // Get a protection status, i.e. wether we may write to the image.
  // Returns true in case we are protected.
  virtual bool ProtectionStatus(void)
  {
    return true; // always
  }
  //
  // Reads data from the image into the buffer from the given
  // byte offset. Returns a boolean success indicator, does not
  // throw.
  virtual bool Read(ULONG offset,void *buffer,ULONG size);
  //
  // Writes data back into the image to the given offset.
  virtual bool Write(ULONG,const void *,ULONG)
  {
    return false;
  }
};
///

///
#endif
