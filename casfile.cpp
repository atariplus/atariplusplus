/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: casfile.cpp,v 1.5 2014/02/23 10:41:08 Administrator Exp $
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
  : src(is), inbuf(0), bytecnt(0)
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

/// CASFile::CreateCASHeader
// Create the header for a CAS file.
void CASFile::CreateCASHeader(void)
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

/// CASFile::Get
// Byte-wise access to the contents, ignoring the IRGs
// and the baud rate. This is the "cooked" access for
// disk emulation.
UBYTE CASFile::Get(void)
{
  do {
    if (inbuf < bytecnt) {
      return buffer[inbuf++];
    } else {
      UWORD irg; // ignored.
      UWORD size = ReadChunk(buffer,sizeof(buffer),irg);
      int i;
      UBYTE chksum;
      
      if (size == 0)
	throw 0; // The EOF condition is indicated by throwing an int.
      
      // Initialize the input buffer pointer and the size of the buffer.
      inbuf = 3;
      // Skip the buffer header.
      if (buffer[0] != 0x55 && buffer[1] != 0x55)
	Throw(InvalidParameter,"CASFile::Get","invalid CAS chunk, sync marker missing");
      switch(buffer[2]) {
      case 0xfc: // A full chunk
	bytecnt = size - 1; // not the checksum
	break;
      case 0xfa: // A partial chunk.
	bytecnt = buffer[size - 2] + 3;
	if (bytecnt >= size - 1)
	  Throw(InvalidParameter,"CASFile::Get","invalid CAS length indicator");
	break;
      case 0xfe: // An EOF chunk.
	bytecnt = 0;
	break;
      default:
	Throw(InvalidParameter,"CASFile::Get","invalid CAS chunk type");
      }
      // Compute the checksum.
      for(i = 0,chksum = 0;i < size - 1;i++) {
	if (buffer[i] + int(chksum) >= 256) {
	  chksum += buffer[i] + 1; // also add the carry;
	} else {
	  chksum += buffer[i];
	}
      }
      if (chksum != buffer[size - 1])
	Throw(InvalidParameter,"CASFile::Get","CAS chunk checksum is invalid");
      //
      // Everything is now initialized. Just throw on EOF now.
      if (buffer[2] == 0xfe)
	throw 0;
    }
  } while(true);
}
///

