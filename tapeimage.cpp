/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: tapeimage.cpp,v 1.3 2015/09/20 21:01:56 thor Exp $
 **
 ** Abstraction for anything that can go into the emulated tape drive: CAS files
 ** and WAV samples that require decoding
 **********************************************************************************/

/// Includes
#include "tapeimage.hpp"
#include "exceptions.hpp"
#include "stdio.hpp"
#include "casfile.hpp"
#include "wavdecoder.hpp"
///

/// TapeImage::TapeImage
TapeImage::TapeImage(void)
  : inbuf(0), bytecnt(0)
{ }
///

/// TapeImage::Get
// Byte-wise access to the contents, ignoring the IRGs
// and the baud rate. This is the "cooked" access for
// disk emulation.
UBYTE TapeImage::Get(void)
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

/// TapeImage::CreateImageForFile
// Create the suitable reader for a given file.
class TapeImage *TapeImage::CreateImageForFile(class Machine *mach,FILE *file)
{
  UBYTE hdr[4];
  //
  // Two possibilities exist here: Tape archives as CAS files with FUJI as header
  // or WAV files with the RIFF header.
  errno = 0;
  if (fread(hdr,sizeof(UBYTE),4,file) != 4) {
    if (errno) {
      ThrowIo("TapeImage::CreateImageForFile","cannot read the tape image header bytes");
    } else {
      throw AtariException("unexpected EOF","TapeImage::CreateImageForFile","cannot read the tape image header bytes");
    }
  }

  if (fseek(file,-4,SEEK_CUR) != 0)
    ThrowIo("TapeImage::CreateImageForFile","unable to rewind the archive");
  
  if (hdr[0] == 'F' && hdr[1] == 'U' && hdr[2] == 'J' && hdr[3] == 'I') {
    return new CASFile(file);
  } else if (hdr[0] == 'R' && hdr[1] == 'I' && hdr[2] =='F' && hdr[3] == 'F') {
    return new WavDecoder(mach,file);
  } else {
    Throw(InvalidParameter,"TapeImage::CreateImageForFile",
	  "The file is neither a CAS image nor a WAV file and cannot be used to feed the tape drive");
  }
  
  return NULL; // code does not go here.
}
///
