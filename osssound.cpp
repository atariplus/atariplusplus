/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: osssound.cpp,v 1.41 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Os interface towards sound output by Oss
 **********************************************************************************/

/// Includes
#include "types.h"
#include "sound.hpp"
#include "machine.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "timer.hpp"
#include "pokey.hpp"
#include "exceptions.hpp"
#include "osssound.hpp"
#include "unistd.hpp"
#include "new.hpp"
#ifdef USE_SOUND
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#endif
#include <errno.h>
///

/// OssSound::OssSound
OssSound::OssSound(class Machine *mach)
  : Sound(mach), DspName(new char[9]), SoundStream(-1),
    Divisor(15700), FragSize(10), NumFrags(4), BufferSize(0), ForceStereo(false)
{
  strcpy(DspName,"/dev/dsp");
}
///

/// OssSound::~OssSound
OssSound::~OssSound(void) 
{
  // get rid of all data within here.
#ifdef USE_SOUND
  if (SoundStream >= 0) {
    close(SoundStream);
  }
#endif
  delete[] DspName;
  //
  // Audio buffers get disposed in the 
  // parent class "Sound"
}
///

/// OssSound::ColdStart
// Run a coldstart. Also initializes the dsp device if we haven't done so
// before. Or at least, it tries to.
void OssSound::ColdStart(void)
{
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  //
  // Check whether the user requested output. If so, try to configure the
  // dsp for output.
#ifdef USE_SOUND
  if (EnableSound && SoundStream < 0) {
    if (!InitializeDsp()) {
      // opening or configuring /dev/dsp failed. Do not try again!
      EnableSound = false;
    }
  }
#else
  EnableSound = false;
#endif
  //
  // Now run for the warmstart.
  WarmStart();
}
///

/// OssSound::WarmStart
// Run a simple reset here.
void OssSound::WarmStart(void)
{
  ConsoleSpeakerStat = false;
}
///

/// OssSound::ConsoleSpeaker
// Turn the console speaker on or off:
void OssSound::ConsoleSpeaker(bool onoff)
{
  ConsoleSpeakerStat = onoff;
  UpdateSound(NULL);
}
///

/// OssSound::InitializeDsp
// Opens the sound device /dev/dsp and installs all
// user parameters here. Call this only if the sound
// is enabled.
// Return true on success, false on error.
bool OssSound::InitializeDsp(void)
{
#ifdef USE_SOUND
  unsigned int formats,fragsize;
  int channels;
  //
  if (SoundStream < 0) {
    // o.k., the stream is not yet open. Do now.
    SoundStream = open(DspName,O_WRONLY | O_NONBLOCK,0777);
    //
    // If this didn't work out, print a warning message for the
    // user.
    if (SoundStream < 0) {
      machine->PutWarning("Audio Setup:\n"
			  "Couldn't open %s for audio output, disabling it for now.\n"
			  "For the next time, either make %s available or disable the\n"
			  "sound output.\nFailure: %s\n",
			  DspName,DspName,strerror(errno));
      return false;
    }
  }
  //
  // Setup the fragment size. This is accoring to the Oss manuals the
  // very first thing we should do.
  fragsize = (NumFrags<<16)|FragSize; // n fragments each of this size
  if (ioctl(SoundStream,SNDCTL_DSP_SETFRAGMENT,&fragsize) < 0) {
    ThrowIo("OssSound::InitializeDsp","Cannot setup the fragment specification");
  }
  //
  // Now initialize us by IoCtrl. Set the current audio format, try
  // unsigned char first. These may fail as LONG as the result is
  // fine when quering the state.
  //
  formats = AFMT_U8;
  if (ioctl(SoundStream,SNDCTL_DSP_SETFMT, &formats) < 0) {
    // try again with signed
    formats = AFMT_S8;
    ioctl(SoundStream,SNDCTL_DSP_SETFMT, &formats);
  }
  //
  // Now query the current format
  //
  formats = AFMT_QUERY;
  if (ioctl(SoundStream,SNDCTL_DSP_SETFMT, &formats) < 0) {
    ThrowIo("OssSound::InitializeDsp","Cannot query the output sample format.");
  }
  // Check the channel formats. We support the most popular.
  switch(formats) {
  case AFMT_U8:
    SignedSamples = false;
    SixteenBit    = false;
    LittleEndian  = false;
    break;
  case AFMT_S8:
    SignedSamples = true;
    SixteenBit    = false;
    LittleEndian  = false;
    break;
  case AFMT_S16_LE: // signed 16 bit little endian
    SignedSamples = true;
    SixteenBit    = true;
    LittleEndian  = true;
    break;
  case AFMT_S16_BE: // signed 16 bit big endian
    SignedSamples = true;
    SixteenBit    = true;
    LittleEndian  = false;
    break;
  case AFMT_U16_LE: // unsigned 16 bit little endian
    SignedSamples = false;
    SixteenBit    = true;
    LittleEndian  = true;
    break;
  case AFMT_U16_BE: // unsigned 16 bit bit endian
    SignedSamples = false;
    SixteenBit    = true;
    LittleEndian  = false;
    break;
  default:
    Throw(InvalidParameter,"OssSound::InitializeDsp","Unknown audio sample format");
  }
  //
  // Set the number of channels for the audio output. We only need mono or stereo.
  channels = (RightPokey)?1:0;
  if (ForceStereo)
    channels = 1;
  if (ioctl(SoundStream,SNDCTL_DSP_STEREO,&channels) < 0) {
    ThrowIo("OssSound::InitializeDsp","Cannot set the audio output to mono");
  };
  switch (channels) {
  case 1:
    if (RightPokey) {
      Stereo      = false; // this is actually a channel duplication bug
      Interleaved = true;
    } else {
      Stereo      = true; // only stereo output supported
      Interleaved = false;
    }
    break;
  case 0:
    Stereo      = false;
    Interleaved = false;
    break;
  default:
    Throw(InvalidParameter,"OssSound::InitializeDsp","Unsupported number of channels");
    break;
  }
  //
  // Set the DSP sample rate to the user preferred value.
  // This is optional as LONG as we can read the rate back.
  if (ioctl(SoundStream,SNDCTL_DSP_SPEED,&SamplingFreq) < 0) {
    ThrowIo("OssSound::InitializeDsp","Cannot set the audio sampling rate");
  }
  //
  // 
  // Read the rate back. This must work.
  if (ioctl(SoundStream,SOUND_PCM_READ_RATE, &SamplingFreq) < 0) {
    ThrowIo("OssSound::InitializeDsp","Cannot figure out the audio sampling rate");
  }
  //
  // Now query the used size of a fragment. This might be something different
  // than what we asked for.
  if (ioctl(SoundStream,SNDCTL_DSP_GETBLKSIZE,&fragsize) < 0) {
    ThrowIo("OssSound::InitializeDsp","Cannot figure out the active buffer size");
  };
  // Compute the buffer size and allocate it.
  BufferSize      = SamplingFreq / Divisor;
  //
  // Allocate a new playing buffer with these settings.
  delete PlayingBuffer;
  PlayingBuffer = NULL;
  PlayingBuffer = AudioBufferBase::NewBuffer(SignedSamples,Stereo,SixteenBit,LittleEndian,Interleaved);
  PlayingBuffer->Realloc(BufferSize);
  //
  //
  return true;
#else
  return false;
#endif
}
///

/// OssSound::UpdateSound
// Update the output sound, feed new data into the DSP.
// Delay by the timer or don't delay at all if no 
// argument given.
void OssSound::UpdateSound(class Timer *delay)
{
  // Check whether we want the sound at all. In case we don't, we
  // do not need all this and delay with the timer.
#ifdef USE_SOUND
  bool dspready;
  if (EnableSound) {
    do {
      // Check whether we must wait for a given time now.
      if (delay) {
	dspready = delay->WaitForIO(SoundStream);
      } else {
	// Note that CheckIO is a static method.
	dspready = delay->CheckIO(SoundStream);
      }
      // If the dsp is ready to take more data, feed it.
      if (dspready) {
	int disp;
	UBYTE offset = 0;
	// Compute/get the console speaker.
	if (EnableConsoleSpeaker && ConsoleSpeakerStat) {
	  offset = ConsoleVolume;
	}
	// Ask pokey to compute more samples into the playing buffer.
	PlayingBuffer->WritePtr = PlayingBuffer->Buffer;
	LeftPokey->ComputeSamples(PlayingBuffer,BufferSize,SamplingFreq,offset);
	// Check whether we have an interleaved buffer. If so, then we must 
	// run the second pokey.
	if ((disp = PlayingBuffer->ChannelOffset())) {
	  PlayingBuffer->WritePtr  = PlayingBuffer->Buffer + disp;
	  RightPokey->ComputeSamples(PlayingBuffer,BufferSize,SamplingFreq,offset);
	  PlayingBuffer->WritePtr -= disp;
	}
	//
	// Write into the audio output device.
	errno = 0;
	if (write(SoundStream,PlayingBuffer->ReadPtr,PlayingBuffer->ReadyBytes()) == -1) {
	  if (errno != EAGAIN) {
	    // Abort if there has been an error. Abort if so.
	    ThrowIo("OssSound::UpdateSound","Writing samples to the audio stream failed.");
	    EnableSound = false;
	  }
	}
      }
      // Even if we could push more data, abort here if we have no timer
      if (delay == NULL) break;
    } while(dspready);
  } else {
    // Sound has been disabled. Just do the wait if we have to wait.
    if (delay) {
      delay->WaitForEvent();
    }
  }
#else
  if (delay)
    delay->WaitForEvent();
#endif
}
///

/// OssSound::HBI
// Let the sound driver know that 1/15Khz seconds passed.
// This might be required for resynchronization of the
// sound driver.
void OssSound::HBI(void)
{
}
///

/// OssSound::DisplayStatus
// Display the status of the sound over the monitor
void OssSound::DisplayStatus(class Monitor *mon)
{
#ifdef USE_SOUND
  mon->PrintStatus("Audio Output Status:\n"
		   "\tAudio output enable     : %s\n"
		   "\tConsole speaker enable  : %s\n"
		   "\tConsole speaker volume  : " LD "\n"
		   "\tAudio output device     : %s\n"
		   "\tSampling frequency      : " LD "Hz\n"
		   "\tBuffer refill frequency : " LD "Hz\n"
		   "\tFragment size exponent  : " LD "\n"
		   "\tNumber of fragments     : " LD "\n"
		   "\tChannel duplication     : %s\n"
		   "\tStereo sound            : %s\n"
		   "\tChannel bit depth       : %d\n"
		   "\tAudio data is           : %s\n",
		   (EnableSound)?("on"):("off"),
		   (EnableConsoleSpeaker)?("on"):("off"),
		   ConsoleVolume,
		   DspName,
		   SamplingFreq,
		   Divisor,
		   FragSize,
		   NumFrags,
		   Stereo?("on"):("off"),
		   Interleaved?("on"):("off"),
		   SixteenBit?16:8,
		   SignedSamples?("signed"):("unsigned")
		   );
#else
  mon->PrintStatus("Audio Output Status:\n"
		   "\tAudio not compiled in\n");
#endif
}
///

/// OssSound::ParseArgs
// Parse off command line arguments for the sound handler
void OssSound::ParseArgs(class ArgParser *args)
{
#ifdef USE_SOUND
  bool enable = EnableSound;
  
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  
  args->DefineTitle("OssSound");
  args->DefineBool("EnableSound","enable audio output",enable);
  args->DefineBool("EnableConsoleSpeaker","enable the console speaker",
		   EnableConsoleSpeaker);
  args->DefineBool("ForceStereo","enforce stereo output for broken ALSA interfaces",ForceStereo);
  args->DefineLong("ConsoleSpeakerVolume","set volume of the console speaker",
		   0,64,ConsoleVolume);
  args->DefineString("AudioDevice","set audio output device",DspName);
  args->DefineLong("SampleFreq","set audio sampling frequency",
		   4000,48000,SamplingFreq);
  args->DefineLong("RefillFreq","set audio buffer refill frequency",
		   20,SamplingFreq,Divisor);
  args->DefineLong("FragSize","set the exponent of the fragment size",
		   2,16,FragSize);
  args->DefineLong("NumFrags","specify the number of fragments",
		   1,256,NumFrags);  
  
  // Re-read the base frequency
  PokeyFreq = LeftPokey->BaseFrequency();
  // Close the sound stream to be able to re-open it.
  if (SoundStream >= 0) {
    close(SoundStream);
    SoundStream = -1;
  }    
  if (enable) {
    EnableSound = true;
    if (!InitializeDsp()) {
      // opening or configuring /dev/dsp failed. Do not try again!
      EnableSound = false;
    }
  } else {
    EnableSound = false;
  }
#else
  EnableSound = false;
#endif
}
///

