/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: zstream.hpp,v 1.6 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: Disk image source stream interface towards zlib
 **********************************************************************************/

#ifndef ZSTREAM_HPP
#define ZSTREAM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "imagestream.hpp"
#include "list.hpp"
#include "new.hpp"
#if HAVE_ZLIB_H && HAVE_GZOPEN
#include <zlib.h>
///

/// Class ZStream
// This class implements the image stream interface for ZLib compressed
// IO streams.
class ZStream : public ImageStream {
  //
  // The file we manage as input file.
  gzFile     InFile;
  //
  // The size of the image, decompressed
  ULONG      Size;
  //
  // The decompressed contents: This is organized as buffers of 
  // 256 bytes each (more or less one for each sector pair)
  struct Buffer : public Node<struct Buffer> {
    // Offset within the buffer of this node.
    ULONG  Offset;
    // Data contents for this buffer.
    UBYTE *Data;
    //
    Buffer(void)
      : Offset(0), Data(new UBYTE[256])
    { }
    ~Buffer(void)
    {
      delete[] Data;
    }
  };
  // Now, the contents is a list of buffers of the above
  // type.
  List<Buffer> Contents;
  //
public:
  ZStream(void);
  virtual ~ZStream(void);
  //
  // Open the image from a file name.
  virtual void OpenImage(const char *filename);
  //
  // Format an image. Since we cannot write, we do 
  // return an error.
  virtual bool FormatImage(const char *)
  {
    return false;
  }
  //
  // Get the size of an image in bytes.
  virtual ULONG ByteSize(void)
  {
    return Size;
  }
  //
  // Get a protection status, i.e. wether we may write to the image.
  // Returns true in case we are protected. Since we cannot write back
  // to the compressed image without recompression, we currently 
  // return true here: always protected
  virtual bool ProtectionStatus(void)
  {
    return true;
  }
  //
  // Reads data from the image into the buffer from the given
  // byte offset. Returns a boolean success indicator, does not
  // throw.
  virtual bool Read(ULONG offset,void *buffer,ULONG size);
  //
  // Writes data back into the image to the given offset.
  virtual bool Write(ULONG offset,const void *buffer,ULONG size);
};
#endif
///

///
#endif
