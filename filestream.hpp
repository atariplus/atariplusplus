/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filestream.hpp,v 1.5 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Disk image source stream interface towards stdio
 **********************************************************************************/

#ifndef FILESTREAM_HPP
#define FILESTREAM_HPP

/// Includes
#include "types.hpp"
#include "imagestream.hpp"
#include "stdio.hpp"
///

/// Class FileStream
// This class implements the image stream interface for standard I/O
// (ANSI) streams.
class FileStream : public ImageStream {
  //
  // The file we manage
  FILE *File;
  //
  // The size of the image
  ULONG Size;
  //
  // The protection status of the file.
  bool  IsProtected;
  //
public:
  FileStream(void);
  virtual ~FileStream(void);
  //
  // Open the image from a file name.
  virtual void OpenImage(const char *filename);
  //
  // Re-format a file stream, i.e. open the file for writing
  // and set its size to unlimited. Returns a boolean indicator
  // for success rather than throwing.
  virtual bool FormatImage(const char *filename);
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
    return IsProtected;
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
///

///
#endif
