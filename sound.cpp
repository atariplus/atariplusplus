/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sound.cpp,v 1.17 2003/05/29 18:14:02 thor Exp $
 **
 ** In this module: Os interface towards sound output
 **********************************************************************************/

/// Includes
#include "sound.hpp"
#include "machine.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "timer.hpp"
#include "pokey.hpp"
#include "exceptions.hpp"
#include "vbiaction.hpp"
#include "audiobuffer.hpp"
///

/// Sound::Sound
Sound::Sound(class Machine *mach)
  : Chip(mach,"Sound"), VBIAction(mach), HBIAction(mach),
    LeftPokey(NULL), RightPokey(NULL),
    PokeyFreq(15700), // defaults to the even NTSC frequency base
    SignedSamples(false), Stereo(false), SixteenBit(false),
    LittleEndian(false), Interleaved(false), SamplingFreq(44100),
    ConsoleSpeakerStat(false), PlayingBuffer(NULL),
    EnableSound(true), EnableConsoleSpeaker(true), ConsoleVolume(32)
{
}
///

/// Sound::~Sound
Sound::~Sound(void) 
{
  CleanBuffer();
}
///

/// Sound::GenerateSamples
// Generate the given number (not in bytes, but in number) of audio samples
// and place them into the tail of the ready buffer list.
// Make buffers at least fragsize samples large.
ULONG Sound::GenerateSamples(ULONG numsamples,ULONG fragsize)
{
  struct AudioBufferBase *ab;
  UBYTE offset,*writeptr;
  int disp;
  //
  // Check whether we still have a buffer pending that could take the samples. 
  // The candidate is the tail of the ready buffer list.
  ab = ReadyBuffers.Last();
  if (ab == NULL || ab->FreeSamples() < numsamples) {
    ULONG newsize = numsamples;
    // No, we cannot extend this buffer. Get a new one and make it at
    // least as large as a fragment.
    if (newsize < fragsize) {
      newsize = fragsize;
    }
    // Remove the next available audio buffer from the free-list, or rebuild one.
    ab = FreeBuffers.RemHead();
    if (ab == NULL) {
      ab = AudioBufferBase::NewBuffer(SignedSamples,Stereo,SixteenBit,LittleEndian,Interleaved);
    }   
    //
    // Queue this into the tail of the audio device output queue immediately as not to
    // loose it if someone throws.  
    ReadyBuffers.AddTail(ab);
    ab->Realloc(newsize);
  }
  // Compute the offset for the console speaker.
  offset = 0;
  if (EnableConsoleSpeaker && ConsoleSpeakerStat) {
    offset = UBYTE(ConsoleVolume);
  }
  // One way or another, we have now a sample buffer. 
  // Now ask pokey to compute more samples. This also adds the console speaker offset
  // right away.
  writeptr = ab->WritePtr;
  LeftPokey->ComputeSamples(ab,numsamples,SamplingFreq,offset);
  // If we are generating interleaved samples, we must have a second pokey for that
  // and now fill in the other half of the samples.
  if ((disp = ab->ChannelOffset())) {
    ab->WritePtr  = writeptr + disp;
    RightPokey->ComputeSamples(ab,numsamples,SamplingFreq,offset);
    ab->WritePtr -= disp;
  }
  //
  // Now return the total number of generated samples.
  return numsamples;
}
///

/// Sound::CleanBuffer
// Cleanup the buffer for the next go
void Sound::CleanBuffer(void)
{  
  struct AudioBufferBase *ab;
  // Dispose the audio buffer nodes now.
  while((ab = ReadyBuffers.RemHead()))
    delete ab;
  while((ab = FreeBuffers.RemHead()))
    delete ab;  
  delete PlayingBuffer;
  PlayingBuffer = NULL;
}
///

/// Sound::VBI
// On VBI, provided we aren't late, update the sound.
// This implements the interface of the VBIAction class.
// This is the one and only class in the VBI chain that finally
// is allowed to time something.
void Sound::VBI(class Timer *time,bool quick,bool pause)
{
  if (quick == false) {
    if (pause) {
      // No sound output on a paused machine
      time->WaitForEvent();
    } else {
      UpdateSound(time);
    }
  }
}
///