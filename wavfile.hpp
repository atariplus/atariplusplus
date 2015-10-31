/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: wavfile.hpp,v 1.4 2015/09/26 13:22:46 thor Exp $
 **
 ** In this module: Support for WAV files describing tape recordings
 **********************************************************************************/

#ifndef WAVFILE_HPP
#define WAVFILE_HPP

/// Includes
#include "types.hpp"
#include "stdio.hpp"
#include "exceptions.hpp"
#include <assert.h>
///

/// WavFile
// A simple helper structure to read and analyze a wav file.
class WavFile {
  //
  // The file to be parsed.
  FILE *m_pSource;
  //
  // Full size of the file.
  ULONG m_ulTotalSize;
  //
  // Number of channels in the file.
  UWORD m_usNumChannels;
  //
  // The recording frequency of the file.
  ULONG m_ulFrequency;
  //
  // Bits per channels.
  UWORD m_usBitsPerChannel;
  //
  // Total number of samples in the file.
  ULONG m_ulSampleCount;
  //
  // Current left and right sample value.
  WORD  m_sLeftSample;
  WORD  m_sRightSample;
  //
  // Read a single character.
  UBYTE Get(void) const
  {
    int in = fgetc(m_pSource);

    if (feof(m_pSource))
      throw AtariException(0,"unexpected EOF","WavFile::Get",
			   "Unexpected EOF while reading from the the WAV file");

    if (ferror(m_pSource))
      ThrowIo("WavFile::Get",
	      "Unexpected error while reading from the WAV file");
    
    return UBYTE(in);
  }
  //
  // Get a 16 bit quantity (little endian), return it.
  UWORD GetWord(void) const
  {
    UWORD lo = Get();
    UWORD hi = Get();

    return lo | (hi << 8);
  }
  //
  // Get a 32 bit quantity (little endian)
  ULONG GetLong(void) const
  {
    ULONG lo = GetWord();
    ULONG hi = GetWord();

    return lo | (hi << 16);
  }
  //
  // Write a single byte.
  void Write(UBYTE d) const
  {
    fputc(d,m_pSource);
  }
  //
  // Write a word, little-endian.
  void WriteWord(UWORD d) const
  {
    Write(UBYTE(d));
    Write(UBYTE(d >> 8));
  }
  //
  // Write a long-word, little-endian.
  void WriteLong(ULONG d) const
  {
    WriteWord(UWORD(d));
    WriteWord(UWORD(d >> 16));
  }
  //
  // Read a chunk ID
  ULONG ReadID(void) const
  {
    char i1,i2,i3,i4;

    i1 = Get();
    i2 = Get();
    i3 = Get();
    i4 = Get();

    return MakeID(i1,i2,i3,i4);
  }
  //
  // Write a four-byte ID string.
  void WriteID(ULONG id) const
  {
    fputc(id >> 24,m_pSource);
    fputc(id >> 16,m_pSource);
    fputc(id >>  8,m_pSource);
    fputc(id >>  0,m_pSource);
  }
  //
  // Create an ID from four UBYTEs.
  static ULONG MakeID(char i1,char i2,char i3,char i4) 
  {
    return ((ULONG(i1) << 24) |
	    (ULONG(i2) << 16) |
	    (ULONG(i3) <<  8) |
	    (ULONG(i4) <<  0));
  }
  //
  // Skip the indicated number of bytes in the file.
  void SkipBytes(ULONG bytes) const
  {
    assert(bytes < (1UL << 31));

    if (fseek(m_pSource,bytes,SEEK_CUR) < 0)
      ThrowIo("WavFile::SkipBytes",
	      "Unexpected I/O error when skipping bytes, probably an End of File");

  }
  //
public:
  WavFile(FILE *source)
  : m_pSource(source), m_ulTotalSize(0), m_usNumChannels(0), m_ulFrequency(0),
    m_ulSampleCount(0)
  { }
  //
  // Parse the header, fill in all the details.
  void ParseHeader(void);
  //
  // Write the wav header. Generate 8-bit mono samples with the given sampling
  // frequency.
  void WriteHeader(ULONG freq);
  //
  // Get the left sample value
  WORD LeftSample(void) const
  {
    return m_sLeftSample;
  }
  //
  // Get the right sample value
  WORD RightSample(void) const
  {
    return m_sRightSample;
  }
  //
  // Write a single mono sample.
  void WriteSample(UBYTE sample)
  {
    Write(sample);
    m_ulSampleCount++;
  }
  //
  // Write a scaled double sample in the range -1..1
  void WriteSample(double sample)
  {
    BYTE out;
    
    sample *= 127.0;

    if (sample < -128.0) {
      out = -128;
    } else if (sample > 127.0) {
      out = 127;
    } else {
      out = BYTE(sample);
    }

    WriteSample(UBYTE(out + 128));
  }
  //
  // Advance to the next sample value for reading if possible.
  // Return true if there are more samples to follow, false otherwise.
  bool Advance(void);
  //
  // Return the sample frequency
  ULONG FrequencyOf(void) const
  {
    return m_ulFrequency;
  }
  //
  // Normalize a sample.
  double Normalize(WORD sample)
  {
    switch(m_usBitsPerChannel) {
    case 8:
      return (sample - 128) / 128.0;
    case 16:
      return sample / 32768.0;
    default:
      return 0.0;
    }
  }
  //
  // Return the number of samples still in the chunk.
  ULONG RemainingSamples(void) const
  {
    return m_ulSampleCount;
  }
  //
  // Complete the WAV file by writing the missing length information
  void CompleteFile(void);
};
///

///
#endif

