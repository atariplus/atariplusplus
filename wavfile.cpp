/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: wavfile.cpp,v 1.2 2015/09/20 15:47:16 thor Exp $
 **
 ** In this module: Support for WAV files describing tape recordings
 **********************************************************************************/

/// Includes
#include "wavfile.hpp"
#include "stdio.hpp"
#include "exceptions.hpp"
#include <assert.h>
///

/// WavFile::ParseHeader
// Parse the header, fill in all the details.
void WavFile::ParseHeader(void)
{
  ULONG chunk_id;
  ULONG chunk_size;

  assert(m_ulTotalSize == 0);
  
  if (ReadID() != MakeID('R','I','F','F'))
    Throw(InvalidParameter,"WavFile::ParseHeader",
	  "Invalid input file - file is not a WAV file");

  m_ulTotalSize = GetLong();
  
  if (ReadID() != MakeID('W','A','V','E'))
    Throw(InvalidParameter,"WavFile::ParseHeader",
	  "Invalid RIFF file type, must be WAVE");

  do {
    chunk_id   = ReadID();
    chunk_size = GetLong();
    if (chunk_id == MakeID('f','m','t',' ')) {
      UWORD blockalign;
      ULONG byterate;

      if (m_usNumChannels != 0)
	Throw(NotImplemented,"WavFile::ParseHeader",
	      "Found multiple fmt chunks in WAV file - not supported by this reader");
      // The format chunk.
      if (chunk_size < 16)
	Throw(InvalidParameter,"WavFile::ParseHeader",
	      "Malformed fmt chunk in WAV file, must be at least 16 bytes long");
      if (GetWord() != 1)
	Throw(NotImplemented,"WavFile::ParseHeader",
	      "Unsupported WAV sample type, must be PCM = 1");
      m_usNumChannels = GetWord();
      if (m_usNumChannels != 1 && m_usNumChannels != 2)
	Throw(NotImplemented,"WavFile::ParseHeader",
	      "Number of channels in the file must be 1 or 2, other values are not supported");
      m_ulFrequency = GetLong();
      if (m_ulFrequency == 0)
	Throw(NotImplemented,"WavFile::ParseHeader",
	      "Found an unknown sample frequency in the WAV file, must be > 0");
      byterate   = GetLong();
      blockalign = GetWord();
      m_usBitsPerChannel = GetWord();
      
      if (m_usBitsPerChannel != 8 && m_usBitsPerChannel != 16)
	Throw(NotImplemented,"WavFile::ParseHeader",
	      "Unsupported number of bits per channel in WAV file, must be 8 or 16");
      
      if (blockalign != (m_usNumChannels * m_usBitsPerChannel) >> 3)
	Throw(InvalidParameter,"WavFile::ParseHeader",
	      "Indicated block alignment is invalid, corrupt WAV file");

      if (byterate != (m_ulFrequency * m_usNumChannels * m_usBitsPerChannel) >> 3)
	Throw(InvalidParameter,"WavFile::ParseHeader",
	      "Indicated byte rate is invalid, corrupt WAV file");

      if (chunk_size > 16)
	SkipBytes(chunk_size - 16);
    } else if (chunk_id == MakeID('d','a','t','a')) {
      ULONG divide = (m_usNumChannels * m_usBitsPerChannel) >> 3;
      if ((chunk_size % divide) != 0)
	Throw(InvalidParameter,"WavFile::ParseHeader",
	      "WAV file invalid, data chunk size is not divisible by the number of samples");
      if (chunk_size == 0)
	Throw(InvalidParameter,"WavFile::ParseHeader",
	      "WAV file invalid, data chunk size cannot be zero");
      if (m_ulSampleCount != 0)
	Throw(PhaseError,"WavFile::ParseHeader",
	      "WAV file parsing out of sync, still samples in the current data chunk");
      m_ulSampleCount = chunk_size / divide;
      //
      // Get the first sample.
      Advance();
      break;
    } else {
      // Unknown chunk, just skip.
      SkipBytes(chunk_size);
    }
  } while(true);
}
///

/// WavFile::Advance
// Go to the next sample, return true if there is one.
bool WavFile::Advance(void)
{
  if (m_ulSampleCount > 0) {
    try {
      if (m_usBitsPerChannel == 8) {
	m_sLeftSample = Get();
      } else {
	m_sLeftSample = GetWord();
      }
      if (m_usNumChannels == 1) {
	m_sRightSample = m_sLeftSample;
      } else {
	if (m_usBitsPerChannel == 8) {
	  m_sRightSample = Get();
	} else {
	m_sRightSample = GetWord();
	}
      }
      m_ulSampleCount--;
      return true;
    } catch(AtariException &ae) {
      m_ulSampleCount = 0;
      throw ae;
    }
  }
  return false;
}
///

/// WavFile::WriteHeader
// Write all the necessary header junk of the WAV file.
void WavFile::WriteHeader(ULONG freq)
{
  assert(freq > 0);
  assert(m_ulFrequency == 0);

  m_ulFrequency   = freq;
  m_ulSampleCount = 0;
  WriteID(MakeID('R','I','F','F'));
  //
  // The chunk size is not yet known, skip it.
  WriteLong(0);
  // Format.
  WriteID(MakeID('W','A','V','E'));
  //
  // Chunk#1, the format.
  WriteID(MakeID('f','m','t',' '));
  WriteLong(16);
  //
  // Encoding: PCM.
  WriteWord(1);
  // Number of channels: Only one.
  WriteWord(1);
  // Sample rate.
  WriteLong(m_ulFrequency);
  // Byte rate. As each sample is a byte, this is identical to the byte rate.
  WriteLong(m_ulFrequency);
  // Block align: One byte per block.
  WriteWord(1);
  // Bits per channel: 8
  WriteWord(8);
  //
  // Chunk#2, the data chunk.
  WriteID(MakeID('d','a','t','a'));
  // The size of the data chunk is not yet known.
  WriteLong(0);
  //
  // Ready so far.
}
///

/// WavFile::CompleteFile
// Complete the file, write the missing length information.
void WavFile::CompleteFile(void)
{
  assert(m_ulFrequency > 0);

  fflush(m_pSource);
  if (fseek(m_pSource,4,SEEK_SET) < 0)
    ThrowIo("WavFile::CompleteFile",
	    "unable to reposition the wav output file to complete the header size");
  
  WriteLong(m_ulSampleCount + 36);
  fflush(m_pSource);

  if (fseek(m_pSource,40,SEEK_SET) < 0)
    ThrowIo("WavFile::CompleteFile",
	    "unable to reposition the wav output file to complete the header size");

  WriteLong(m_ulSampleCount);
}
///
