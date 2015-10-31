/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: audiobuffer.hpp,v 1.8 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: Audio buffer abstraction class provided to collect pokey
 ** output.
 **********************************************************************************/

#ifndef AUDIOBUFFER_HPP
#define AUDIOBUFFER_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "exceptions.hpp"
///

/// Struct AudioBufferBase
// Definition of an audio buffer. This contains the sample data for one block
// we submit to the audio/dsp device.
struct AudioBufferBase : public Node<struct AudioBufferBase> {
  //
  AudioBufferBase(void);
  virtual ~AudioBufferBase(void);
  //
  // The buffer we administrate. Its lifetime is controlled by
  // this structure.
  UBYTE   *Buffer;
  // Pointer to the end of the buffer. For easy overrun-checks.
  UBYTE   *BufferEnd;
  //
  // Read-pointer: That's were we read data from.
  UBYTE   *ReadPtr;
  //
  // Write-pointer: That's where audio data is filled in.
  UBYTE   *WritePtr;
  //
  // Size of this buffer in bytes (not samples). This defines how large
  // the buffer is we allocated.
  ULONG    Size;
  //
  // Compute the bitshift to compute the number bytes per sample, 
  // required for buffer allocation and similar operations.
  UBYTE    SampleShift;
  //
  // This is the call-in required for pokey to place a 
  // single sample into the buffer.
  virtual void PutSample(UBYTE sample) = 0;
  //
  // Return a single sample from the buffer,
  // advance the read pointer accordingly. This is
  // used to convert sample formats.
  virtual UBYTE GetSample(void) = 0;
  //
  // Return the offset from here to the next buffer entry
  // of the other (2nd) channel for interleaved data.
  virtual int ChannelOffset(void) = 0;
  //
  // Re-allocate a buffer for the given number of samples
  void Realloc(ULONG size);
  //
  // Construct a new audio buffer with the given parameters; hence, we provide the
  // factory for the generic type.
  static struct AudioBufferBase *NewBuffer(bool signedsamples,bool stereo,bool sixteenbit,
					   bool littleendian,bool interleaved);
  //
  // Return the number of available bytes for playing.
  ULONG ReadyBytes(void) const
  {
#if CHECK_LEVEL > 0
    if (WritePtr < ReadPtr) {
      Throw(OutOfRange,"AudioBufferBase::ReadyBytes","an empty audio buffer has been detected in the queue");
    }
#endif
    return ULONG(WritePtr - ReadPtr);
  }
  //
  // Return the number of free bytes that can be filled.
  ULONG FreeBytes(void) const
  {
#if CHECK_LEVEL > 0
    if (BufferEnd < WritePtr) {
      Throw(OutOfRange,"AudioBufferBase::FreeBytes","an overrun audio buffer has been detected in the queue");
    }
#endif
    return ULONG(BufferEnd - WritePtr);
  }
  //
  // Return the number of available samples for playing
  ULONG ReadySamples(void) const
  {
    return ReadyBytes() >> SampleShift;
  }
  //
  // Return the number of free samples
  ULONG FreeSamples(void) const
  {
    return FreeBytes() >> SampleShift;
  }
  //
  // Return whether this buffer is empty.
  bool IsEmpty(void) const
  {
    return (ReadPtr >= WritePtr)?(true):(false);
  }
  //
  // Convert the passed in audio buffer into the format of this buffer. 
  // Allocate the new buffer if required.
  void CopyBuffer(struct AudioBufferBase *base);
  //
  // Add an offset to all entries in the buffer, starting at the start of
  // the buffer, up to the ReadPtr. This is required to add a console
  // offset, possibly, afterwards.
  virtual void AddOffset(UBYTE offset) = 0;
  //
  // Check whether all entries between the first entry and the ReadPtr
  // whether the given value equals the passed in value. If not,
  // then it returns true.
  virtual bool CheckForMuting(UBYTE value) = 0;
};
///

/// Templated AudioBuffer
// This buffer is templated by signed-ness, stereo, sixteenbit, big/little endian, and interleaving
// The implementation depends on all these parameters (yuck!).
// Luckely, C++ has generic programming or I'd had to write all this several times.
template<bool signedsamples,bool stereo,bool sixteenbit,bool littleendian,bool interleaved> 
struct AudioBuffer : public AudioBufferBase {
  //   
  // Constructor and destructor are trivial. All done by the base class.
  AudioBuffer(void);
  //
  virtual ~AudioBuffer(void)
  { }
  //
  // This is the call-in required for pokey to place a 
  // single sample into the buffer.
  virtual void PutSample(UBYTE sample);  
  //
  // Return a single sample from the buffer,
  // advance the read pointer accordingly. This is
  // used to convert sample formats.
  virtual UBYTE GetSample(void);  
  //
  // Return the offset from here to the next buffer entry
  // of the other (2nd) channel for interleaved data.
  virtual int ChannelOffset(void);  
  //
  // Add an offset to all entries in the buffer, starting at the start of
  // the buffer, up to the ReadPtr. This is required to add a console
  // offset, possibly, afterwards.
  virtual void AddOffset(UBYTE offset);
  //
  // Check whether all entries between the first entry and the ReadPtr
  // whether the given value equals the passed in value. If not,
  // then it returns true.
  virtual bool CheckForMuting(UBYTE value);
};
///

///
#endif
