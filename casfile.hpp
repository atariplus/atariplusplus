/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: casfile.hpp,v 1.4 2015/09/20 20:22:28 thor Exp $
 **
 ** In this module: Abstraction of CAS files aka "FUJI"
 **********************************************************************************/

#ifndef CASFILE_HPP
#define CASFILE_HPP

/// Includes
#include "types.hpp"
#include "stdio.hpp"
#include "tapeimage.hpp"
///

/// class CASFile
// This class abstracts the FUJI tape image files.
class CASFile : public TapeImage {
  //
  // The image stream to read from.
  FILE               *src;
  //
public:
  // Construct a cas file from a raw file. The file
  // must be administrated outside of this class.
  CASFile(FILE *is);
  //
  virtual ~CASFile(void)
  { }
  //
  // Read from the image into the buffer if supplied.
  // Returns the number of bytes read. Returns also
  // the IRG size in milliseconds.
  virtual UWORD ReadChunk(UBYTE *buffer,LONG buffersize,UWORD &irg);
  //
  // Create a new chunk, write it to the file.
  virtual void WriteChunk(UBYTE *buffer,LONG buffersize,UWORD irg);
  //
  // Create the header for a CAS file.
  virtual void OpenForWriting(void);
};
///

///
#endif

