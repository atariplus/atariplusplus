/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: tapeimage.hpp,v 1.3 2015/10/09 19:20:42 thor Exp $
 **
 ** Abstraction for anything that can go into the emulated tape drive: CAS files
 ** and WAV samples that require decoding
 **********************************************************************************/

#ifndef TAPEIMAGE_HPP
#define TAPEIMAGE_HPP

/// Includes
#include "types.hpp"
#include "stdio.hpp"
///

/// class TapeImage
// This is an abstraction of everything that can hold tapes
class TapeImage {
  //
  // The offset into the data buffer above.
  UBYTE              inbuf;
  //
  // The number of bytes in the input buffer.
  UBYTE              bytecnt;
  //
  // The buffer of the data blocks of the CAS image.
  UBYTE              buffer[256+2+1+1];
  //
public:
  TapeImage(void);
  //
  //
  virtual ~TapeImage(void)
  { }
  //
  // Create the suitable reader for a given file.
  static class TapeImage *CreateImageForFile(class Machine *mach,FILE *file);
  //
  // Read from the image into the buffer if supplied.
  // Returns the number of bytes read. Returns also
  // the IRG size in milliseconds.
  virtual UWORD ReadChunk(UBYTE *buffer,LONG buffersize,UWORD &irg) = 0;
  //
  // Create a new chunk, write it to the file.
  virtual void WriteChunk(UBYTE *buffer,LONG buffersize,UWORD irg) = 0;
  //
  // Close the file for writing.
  virtual void Close(void)
  { };
  //
  // Open a CAS file for writing.
  // This is called after creating the image before the first record is written.
  virtual void OpenForWriting(void)
  { };
  //
  // Open a CAS file for reading
  // This is called after creating the image before the first record is read.
  virtual void OpenForReading(void)
  { };
  //
  // Byte-wise access to the contents, ignoring the IRGs
  // and the baud rate. This is the "cooked" access for
  // disk emulation.
  UBYTE Get(void);
};
///

///
#endif

