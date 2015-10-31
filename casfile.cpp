/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: casfile.cpp,v 1.6 2015/09/20 20:22:28 thor Exp $
 **
 ** In this module: Abstraction of CAS files aka "FUJI"
 **********************************************************************************/

/// Includes
#include "casfile.hpp"
#include "exceptions.hpp"
///

/// CASFile::CASFile
// Construct a cas file from a raw file. The file
// must be administrated outside of this class.
CASFile::CASFile(FILE *is) 
  : src(is)
{
}
///

/// CASFile::ReadChunk
// Read from the image into the buffer if supplied.
// Returns the number of bytes read. Returns also
// the IRG size in milliseconds.
UWORD CASFile::ReadChunk(UBYTE *buffer,LONG buffersize,UWORD &irg)
{  
  UBYTE chunk[8];
  UWORD size;
  //
  do {
    size_t in;
    //
    // read the next chunk from the CAS file.
    errno = 0;
    in = fread(chunk,1,8,src);
    if (in != 8) {
      if (errno) {
	ThrowIo("CASFile::Get","error when reading from CAS file");
      } else {
	if (in == 0) {
	  return 0; // EOF
	} else {
	  Throw(OutOfRange,"CASFile::Get","unexpected EOF when reading from CAS file");
	}
      }
    }
    //
    // Get the size of this chunk.
    size = chunk[4] | (chunk[5] << 8);
    //
    // We only care about data chunks here.
    if (chunk[0] == 'd' && chunk[1] == 'a' && chunk[2] == 't' && chunk[3] == 'a') {
      irg  = chunk[6] | (chunk[7] << 8);
      
      if (size > buffersize)
	Throw(OutOfRange,"CASFile::Get","CAS buffer segment size too large to be read");
      
      if (fread(buffer,1,size,src) != size) {
	if (errno)
	  ThrowIo("CASFile::Get","error when reading from CAS file");
	else
	  Throw(OutOfRange,"CASFile::Get","unexpected EOF when reading from CAS file");
      }	
      
      return size;
    } else {
      // Skip the data.
      while(size) {
	if (fgetc(src) < 0) {
	  if (errno)
	    ThrowIo("CASFile::Get","error when reading from CAS file");
	  else
	    Throw(OutOfRange,"CASFile::Get","unexpected EOF when reading from CAS file");
	}
	size--;
      }
    }
  } while(true);
}
///

/// CASFile::WriteChunk
// Create a new chunk, write it to the file.
void CASFile::WriteChunk(UBYTE *buffer,LONG buffersize,UWORD irg)
{
  char header[8];
 
  if (buffersize > 65535)
    Throw(OutOfRange,"CASFile::WriteChunk","CAS record is too long, can be at most 64K in size");

  header[0] = 'd';
  header[1] = 'a';
  header[2] = 't';
  header[3] = 'a';
  header[4] = UBYTE(buffersize & 0xff);
  header[5] = UBYTE(buffersize >> 8);
  header[6] = irg & 0xff;
  header[7] = irg >> 8;	

  if (fwrite(header,1,sizeof(header),src) != sizeof(header)) {
    ThrowIo("CASFile::WriteChunk","error when writing a record to a CAS file");
  }

  if (fwrite(buffer,1,buffersize,src) != size_t(buffersize)) {
    ThrowIo("CASFile::WriteChunk","error when writing a record to a CAS file");
  }
}
///

/// CASFile::OpenForWriting
// Create the header for a CAS file.
void CASFile::OpenForWriting(void)
{
  char buffer[128];
  const char *description = "Created by Atari++";
  size_t len = strlen(description);
  size_t written;

  buffer[0] = 'F';
  buffer[1] = 'U';
  buffer[2] = 'J';
  buffer[3] = 'I';
  buffer[4] = len & 0xff;
  buffer[5] = len >> 8;
  buffer[6] = 0;
  buffer[7] = 0;
  strcpy(buffer+8,description);

  written = fwrite(buffer,1,len + 8,src);
  if (written != len + 8) {
    ThrowIo("CASFile::Get","error when writing to CAS file");
  }
}
///
