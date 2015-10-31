/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: audiobuffer.cpp,v 1.7 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: Audio buffer abstraction class provided to collect pokey
 ** output.
 **********************************************************************************/

/// Includes
#include "types.hpp"
#include "new.hpp"
#include "audiobuffer.hpp"
///

/// AudioBufferBase::AudioBufferBase
AudioBufferBase::AudioBufferBase(void)
  : Buffer(NULL), BufferEnd(NULL), ReadPtr(NULL), WritePtr(NULL), Size(0)
{
}
///

/// AudioBufferBase::Realloc
// Re-allocate a buffer for the given number of samples
void AudioBufferBase::Realloc(ULONG size)
{
  // First compute how many bytes we need.
  size <<= SampleShift;
  //
  // Then check whether we have to enlarge the buffer.
  if (size > Size) {
    delete[] Buffer;
    Buffer    = NULL;
    Buffer    = new UBYTE[size];
    Size      = size;
    BufferEnd = Buffer + size;
  }
  // Re-seat read and write pointers.
  ReadPtr = WritePtr = Buffer;
}
///

/// AudioBufferBase::~AudioBufferBase
// Dispose an audio buffer
AudioBufferBase::~AudioBufferBase(void)
{
  delete[] Buffer;
}
///

/// AudioBufferBase::NewBuffer
// Construct a new audio buffer with the given parameters; hence, we provide the
// factory for the generic type.
struct AudioBufferBase *AudioBufferBase::NewBuffer(bool signedsamples,bool stereo,bool sixteenbit,
						   bool littleendian,bool interleaved)
{
  // This is unfortunately a bit lengthy since there are so many parameters, but
  // I can't help it.
  if (signedsamples) {
    if (stereo) {
      if (sixteenbit) {
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<true,true,true,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,true,true,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<true,true,true,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,true,true,false,false>;
	  }
	}
      } else { // sixteenbit == false
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<true,true,false,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,true,false,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<true,true,false,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,true,false,false,false>;
	  }
	}
      }
    } else { // stereo == false
      if (sixteenbit) {
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<true,false,true,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,false,true,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<true,false,true,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,false,true,false,false>;
	  }
	}
      } else { // sixteenbit == false
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<true,false,false,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,false,false,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<true,false,false,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<true,false,false,false,false>;
	  }
	}
      }
    }
  } else { // signedsamples == false
    if (stereo) {
      if (sixteenbit) {
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<false,true,true,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,true,true,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<false,true,true,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,true,true,false,false>;
	  }
	}
      } else { // sixteenbit == false
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<false,true,false,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,true,false,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<false,true,false,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,true,false,false,false>;
	  }
	}
      }
    } else { // stereo == false
      if (sixteenbit) {
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<false,false,true,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,false,true,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<false,false,true,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,false,true,false,false>;
	  }
	}
      } else { // sixteenbit == false
	if (littleendian) {
	  if (interleaved) {
	    return new AudioBuffer<false,false,false,true,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,false,false,true,false>;
	  }
	} else { // littleendian == false
	  if (interleaved) {
	    return new AudioBuffer<false,false,false,false,true>;
	  } else { // interleaved == false
	    return new AudioBuffer<false,false,false,false,false>;
	  }
	}
      }
    }
  }
}
///

/// AudioBufferBase::CopyBuffer
// Convert the passed in audio buffer into the format of this buffer. 
// Allocate the new buffer if required.
void AudioBufferBase::CopyBuffer(struct AudioBufferBase *base)
{
  ULONG samples,free,cnt;
  UBYTE *src,*dst;
  int offset;
  //
  samples = base->ReadySamples();
  free    = FreeSamples();
  // Copy at most as many positions as we have free entries!
  if (samples > free)
    samples = free;
  //
  // Now copy the stuff over, but first keep source and target.
  src = base->ReadPtr;
  dst = WritePtr;
  cnt = samples;
  while(cnt) {
    PutSample(base->GetSample());
    cnt--;
  }
  //
  // Check whether the target is interleaved. If so, we need to copy the
  // second buffer as well.
  if ((offset = ChannelOffset())) {
    // Ok, re-seat the read and write source and copy the second part
    // of the data as well.
    base->ReadPtr = src + base->ChannelOffset();
    WritePtr      = dst + offset;
    cnt           = samples;
    while(cnt) {
      PutSample(base->GetSample());
      cnt--;
    }
    base->ReadPtr -= base->ChannelOffset();
    WritePtr      -= offset;
  }
}
///

/// AudioBuffer::AudioBuffer
template<bool signedsamples,bool stereo,bool sixteenbit,bool littleendian,bool interleaved>
AudioBuffer<signedsamples,stereo,sixteenbit,littleendian,interleaved>::AudioBuffer(void)
{ 
  SampleShift = 0;
  //
  // Double the buffer size for stereo, sixteenbit and interleaving.
  if (stereo)      SampleShift++;
  if (sixteenbit)  SampleShift++;
  if (interleaved) SampleShift++;
}
///

/// AudioBuffer::PutSample
// Place a simple sample into the audio buffer. Possibly sign-extend it, enlarge it to
// 16 bit or interleave it.
template<bool signedsamples,bool stereo,bool sixteenbit,bool littleendian,bool interleaved>
void AudioBuffer<signedsamples,stereo,sixteenbit,littleendian,interleaved>::PutSample(UBYTE out)
{
  // Note that all the following are template parameters whose value is known at
  // compile time. Hence, evaluation of these is rather fast since it's all
  // optimized away (hopefully!)
  //
  // First level-shift for unsigned output.
  if (signedsamples == false)
    out += 128;
  //
  // If we have to generate sixteen bit samples, do now.
  // We never generate negative values here, this makes things a bit
  // simpler.
  if (sixteenbit) {
    if (littleendian) {
      *WritePtr++ = 0;     // lo-byte
      *WritePtr++ = out;   // hi-byte
    } else {
      *WritePtr++ = out;   // hi-byte
      *WritePtr++ = 0;     // lo-byte
    }
    if (stereo) {
      if (littleendian) {
	*WritePtr++ = 0;   // lo-byte
	*WritePtr++ = out; // hi-byte
      } else {
	*WritePtr++ = out; // hi-byte
	*WritePtr++ = 0;   // lo-byte
      }
    }
  } else {
    *WritePtr++ = out;
    if (stereo) {
      *WritePtr++ = out;
    }
  }
  // Check whether we are generating interleaved stereo-sound. If so,
  // skip the second sample of the pair.
  if (interleaved) {
    if (sixteenbit) {
      WritePtr += 2;
      if (stereo) {
	WritePtr += 2;
      }
    } else {
      WritePtr++;
      if (stereo) {
	WritePtr++;
      }
    }
  }
}
///

/// AudioBuffer::GetSample
// Return a single sample from the buffer,
// advance the read pointer accordingly. This is
// used to convert sample formats.
template<bool signedsamples,bool stereo,bool sixteenbit,bool littleendian,bool interleaved>
UBYTE AudioBuffer<signedsamples,stereo,sixteenbit,littleendian,interleaved>::GetSample(void)
{
  UBYTE data;
  //
  // Extract the proper data from the samples in the buffer.
  if (sixteenbit) {
    if (littleendian) {
      ReadPtr++;
      data = *ReadPtr++;   // little-endian: Get second hi-byte
    } else {
      data = *ReadPtr++;   // otherwise, the hi-byte is here.
      ReadPtr++;
    }
    if (stereo) {
      // Advance over the second identical sample
      ReadPtr += 2;
    }
  } else {
    data = *ReadPtr++;
    if (stereo) {
      // Advance over the second sample
      ReadPtr++;
    }
  }
  // Check whether we are generating interleaved stereo-sound. If so,
  // skip the second sample of the pair.
  if (interleaved) {
    if (sixteenbit) {
      ReadPtr += 2;
      if (stereo) {
	ReadPtr += 2;
      }
    } else {
      ReadPtr++;
      if (stereo) {
	ReadPtr++;
      }
    }
  }
  //
  // Convert to unsigned if asked so.
  if (signedsamples == false)
    data -= 128;
  //
  return data;
}
///

/// AudioBuffer::ChannelOffset
// Return the offset from here to the next buffer entry
// of the other (2nd) channel for interleaved data.
template<bool signedsamples,bool stereo,bool sixteenbit,bool littleendian,bool interleaved>
int AudioBuffer<signedsamples,stereo,sixteenbit,littleendian,interleaved>::ChannelOffset(void)
{
  if (interleaved) {
    int offset = 1;
    if (stereo)     offset++;
    if (sixteenbit) offset++;
    return offset;
  } else {
    return 0;
  }
}
///

/// AudioBuffer::AddOffset
// Add an offset to all entries in the buffer, starting at the start of
// the buffer, up to the WritePtr. This is required to add a console
// offset, possibly, afterwards.
template<bool signedsamples,bool stereo,bool sixteenbit,bool littleendian,bool interleaved>
void AudioBuffer<signedsamples,stereo,sixteenbit,littleendian,interleaved>::AddOffset(UBYTE offset)
{
  UBYTE *dta = Buffer;

  while(dta < WritePtr) {
    if (sixteenbit) {
      if (littleendian) {
	dta++;
	*dta++ += offset; // only to the high-byte
      } else {
	*dta++ += offset; // only to the high-byte
	dta++;
      }
    } else {
      *dta++ += offset;
    }
    // Whether we are stereo or not does not matter. The same modifications
    // are applied to both buffers.
  }
}
///

/// AudioBuffer::CheckForMuting
// Check whether all entries between the first entry and the WritePtr
// whether the given value equals the passed in value. If not,
// then it returns true.
template<bool signedsamples,bool stereo,bool sixteenbit,bool littleendian,bool interleaved>
bool AudioBuffer<signedsamples,stereo,sixteenbit,littleendian,interleaved>::CheckForMuting(UBYTE value)
{
  UBYTE *dta = Buffer;

  // Check whether we must convert the sample back to unsigned.
  if (signedsamples == false)
    value += 128;
  //
  // Now test in the buffer for a value that is
  // not the muting value.
  while(dta < WritePtr) {
    if (sixteenbit) {
      if (littleendian) {
	dta++; // ignore the lo-byte
	if (*dta++ != value)
	  return true;
      } else {
	if (*dta++ != value)
	  return true;
	dta++;
      }
    } else {
      if (*dta++ != value)
	return true;
    }
    // stereo or interleaved does not matter, we handle them similarly and
    // check both channels.
  }
  
  return false;
}
///
