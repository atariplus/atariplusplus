/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: imagestream.hpp,v 1.5 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Abstract base class for disk image source streams
 **********************************************************************************/

#ifndef IMAGESTREAM_HPP
#define IMAGESTREAM_HPP

/// Includes
#include "types.hpp"
///

/// Class ImageStream
// This base class defines the interface towards generic
// streams that can represent disk images. These streams
// could come from either regular files, or from compressed
// images via the ZLib.
class ImageStream {
  //
public:
  ImageStream(void)
  { }
  virtual ~ImageStream(void)
  { }
  //
  // Open an image given the file name.
  virtual void OpenImage(const char *filename) = 0;
  //  
  // Clean an image stream = truncate it to zero bytes.
  virtual bool FormatImage(const char *filename) = 0;
  //
  // Get the size of an image in bytes.
  virtual ULONG ByteSize(void) = 0;
  //
  // Get a protection status, i.e. wether we may write to the image.
  // Returns true in case we are protected.
  virtual bool ProtectionStatus(void) = 0;
  //
  // Reads data from the image into the buffer from the given
  // byte offset. Returns a boolean success indicator, does not
  // throw.
  virtual bool Read(ULONG offset,void *buffer,ULONG size) = 0;
  //
  // Writes data back into the image to the given offset.
  virtual bool Write(ULONG offset,const void *buffer,ULONG size) = 0;

};
///

///
#endif
