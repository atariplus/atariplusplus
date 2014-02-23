/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: casfile.hpp,v 1.3 2013/06/02 19:18:00 thor Exp $
 **
 ** In this module: Abstraction of CAS files aka "FUJI"
 **********************************************************************************/

#ifndef CASFILE_HPP
#define CASFILE_HPP

/// Includes
#include "types.hpp"
#include "stdio.hpp"
///

/// class CASFile
// This class abstracts the FUJI tape image files.
class CASFile {
  // 
  class Machine      *machine;
  //
  // The image stream to read from.
  FILE               *src;
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
  // Construct a cas file from a raw file. The file
  // must be administrated outside of this class.
  CASFile(FILE *is);
  //
  // Read from the image into the buffer if supplied.
  // Returns the number of bytes read. Returns also
  // the IRG size in milliseconds.
  UWORD ReadChunk(UBYTE *buffer,LONG buffersize,UWORD &irg);
  //
  // Create a new chunk, write it to the file.
  void WriteChunk(UBYTE *buffer,LONG buffersize,UWORD irg);
  //
  // Create the header for a CAS file.
  void CreateCASHeader(void);
  //
  // Byte-wise access to the contents, ignoring the IRGs
  // and the baud rate. This is the "cooked" access for
  // disk emulation.
  UBYTE Get(void);
  //
};
///

///
#endif

