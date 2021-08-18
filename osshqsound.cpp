/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: osshqsound.cpp,v 1.28 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Os interface towards sound output by Oss
 ** with somewhat more quality
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
#include "osshqsound.hpp"
#include "audiobuffer.hpp"
#include "unistd.hpp"
#include "new.hpp"
#ifdef USE_SOUND
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#endif
#include <errno.h>
///

/// HQSound::HQSound
HQSound::HQSound(class Machine *mach)
  : Sound(mach), DspName(new char[9]), SoundStream(-1),
    FragSize(8), NumFrags(16),
    BufferedSamples(0), ForceStereo(false), UpdateBuffer(false), UpdateSamples(0)
{
  strcpy(DspName,"/dev/dsp");
}
///

/// HQSound::~HQSound
HQSound::~HQSound(void) 
{
#ifdef USE_SOUND
  if (SoundStream >= 0) {
    close(SoundStream);
  }
#endif
  delete[] DspName;
  //
  // Audio buffers get disposed by the Sound
}
///

/// HQSound::GenerateSamples
// Generate the given number (not in bytes, but in number) of audio samples
// and place them into the tail of the ready buffer list.
void HQSound::GenerateSamples(ULONG numsamples)
{	
  // Let the parent class do the job
  BufferedSamples += Sound::GenerateSamples(numsamples,FragSamples);
}
///

/// HQSound::FeedDevice
// Feed data into the dsp device by taking buffered bytes from the queue
// and removing the sample buffers, putting them back into the free list.
// This returns false on a buffer underrun
bool HQSound::FeedDevice(class Timer *delay)
{
#ifdef USE_SOUND
  struct AudioBufferBase *ab;
  bool ready;
  
  if (delay) {
    // Wait the time span for the device to become ready.
    ready = delay->WaitForIO(SoundStream);
  } else {
    // Otherwise, just check and bail out if we are not right
    // now available.
    ready = Timer::CheckIO(SoundStream);
  }
  if (ready) {
    // The device is ready to take more samples. Hence, pull them from the ready queue
    ab = ReadyBuffers.RemHead();
    if (ab) {
      // Yes, we have a valid buffer here. Hence, play it.
      errno = 0;
      if (write(SoundStream,ab->ReadPtr,ab->ReadyBytes()) == -1) {
	if (errno != EAGAIN) {      
	  // Put it back into the free list immediately to be able to release the buffer
	  // after the throw.
	  FreeBuffers.AddTail(ab);
	  // Abort if there has been an error. Abort if so.
	  ThrowIo("HQSound::FeedDevice","Writing samples to the audio stream failed.");
	  EnableSound = false;
	} else {
	  // Otherwise, ignore the write and put the sample buffer back
	  // where it came from. EAGAIN indicates that we cannot currently
	  // write into the device without blocking. select() should have ensured
	  // this, but it doesn't.
	  ReadyBuffers.AddHead(ab);
	}
      } else {
	// This buffer is used up. Cound the number of collected samples down, then release
	// the buffer.
	BufferedSamples -= ab->ReadySamples();
	FreeBuffers.AddTail(ab);
      }
      return true;
    }
    return false;
  }
#endif
  return true;
}
///

/// HQSound::ColdStart
// Run a coldstart. Also initializes the dsp device if we haven't done so
// before. Or at least, it tries to.
void HQSound::ColdStart(void)
{
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
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

/// HQSound::WarmStart
// Run a simple reset here.
void HQSound::WarmStart(void)
{
  ConsoleSpeakerStat = false;
  //
  // Dispose the old buffers now.
  CleanBuffer();
  BufferedSamples    = 0;
}
///

/// HQSound::ConsoleSpeaker
// Turn the console speaker on or off:
void HQSound::ConsoleSpeaker(bool onoff)
{
  if (ConsoleSpeakerStat != onoff) {
    ConsoleSpeakerStat = onoff;
    UpdateBuffer       = true;
    UpdateSound(NULL);
  }
}
///

/// HQSound::InitializeDsp
// Opens the sound device /dev/dsp and installs all
// user parameters here. Call this only if the sound
// is enabled.
// Return true on success, false on error.
bool HQSound::InitializeDsp(void)
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
    ThrowIo("HQSound::InitializeDsp","Cannot setup the fragment specification");
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
    ThrowIo("HQSound::InitializeDsp","Cannot query the output sample format.");
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
    Throw(InvalidParameter,"HQSound::InitializeDsp","Unknown audio sample format");
  }
  //
  // Set the number of channels for the audio output. We need mono or stereo.
  channels = (RightPokey)?1:0;
  if (ForceStereo)
    channels = 1;
  if (ioctl(SoundStream,SNDCTL_DSP_STEREO,&channels) < 0) {
    ThrowIo("HQSound::InitializeDsp","Cannot set the audio output to mono");
  };
  switch (channels) {
  case 1:
    if (RightPokey) {
      Stereo      = false; // This is only a channel duplication flag
      Interleaved = true;
    } else {
      Stereo      = true;
      Interleaved = false;
    }
    FragSamples   = (1<<FragSize) >> 1;
    break;
  case 0:
    Stereo        = false;
    Interleaved   = false;
    FragSamples   = 1<<FragSize;
    break;
  default:
    Throw(InvalidParameter,"HQSound::InitializeDsp","Unsupported number of channels");
    break;
  }
  //
  // Set the DSP sample rate to the user preferred value.
  // This is optional as LONG as we can read the rate back.
  if (ioctl(SoundStream,SNDCTL_DSP_SPEED,&SamplingFreq) < 0) {
    ThrowIo("HQSound::InitializeDsp","Cannot set the audio sampling rate");
  }
  //
  // 
  // Read the rate back. This must work.
  if (ioctl(SoundStream,SOUND_PCM_READ_RATE, &SamplingFreq) < 0) {
    ThrowIo("HQSound::InitializeDsp","Cannot figure out the audio sampling rate");
  }
  //
  // Now query the used size of a fragment. This might be something different
  // than what we asked for.
  if (ioctl(SoundStream,SNDCTL_DSP_GETBLKSIZE,&fragsize) < 0) {
    ThrowIo("HQSound::InitializeDsp","Cannot figure out the active buffer size");
  };
  //
  // Setup the effective buffering frequency.
  EffectiveFreq   = SamplingFreq;
  CycleCarry      = 0;
  UpdateBuffer    = false;
  UpdateSamples   = 0;
  //
  return true;
#else
  return false;
#endif
}
///

/// HQSound::HBI
// Let the sound driver know that 1/15Khz seconds passed.
// This might be required for resynchronization of the
// sound driver.
void HQSound::HBI(void)
{
#ifdef USE_SOUND
  if (EnableSound) {
    LONG remaining,samples;
    // Compute the number of samples we need to generate this time.
    remaining      = EffectiveFreq + CycleCarry; // the number of sampling cycles left this time.
    samples        = remaining / PokeyFreq;      // number of samples to generate this time.
    CycleCarry     = remaining - samples * PokeyFreq; // keep the number of samples we did not take due to round-off
    UpdateSamples += samples;
    // Check whether we can avoid the update to reduce the overhead of computing very tiny amounts of data
    // We can't if we collected too many bytes already
    if (UpdateSamples >= FragSamples) {
      UpdateBuffer = true;
    }
    if (UpdateBuffer) {
      // Compute this number of samples, and put it into the buffer.
      GenerateSamples(UpdateSamples);
      UpdateSamples  = 0;
      UpdateBuffer   = false;
    }
  }
#endif
}
///

/// HQSound::UpdateSound
// Update the output sound, feed new data into the DSP.
// Delay by the timer or don't delay at all if no 
// argument given.
void HQSound::UpdateSound(class Timer *delay)
{
  // Check whether we want the sound at all. In case we don't, we
  // do not need all this and delay with the timer.
#ifdef USE_SOUND
  if (EnableSound) {
    // Signal that we must now re-generate some samples as the audio
    // setting changed.
    UpdateBuffer = true;
    // Ok, re-feed the device. In case of underrun, generate some samples manually.
    do {
      while (!FeedDevice(delay)) {
	// Generate one fragment of data.
	GenerateSamples(FragSamples);
	// Note that we underrun. Adjust the effective sampling frequency accordingly.
	AdjustUnderrun();
      }
      // If there is nothing to delay, bail out.
      if (delay == NULL) break;
      // Otherwise, delay until the delay time is over.
    } while(!delay->EventIsOver());
    //
    // Now check for the number of bytes in the buffer. This should not
    // grow endless. If it is too large, signal an overrun and adjust the
    // frequency accordingly.
    if (BufferedSamples > ULONG(NumFrags-2) * FragSamples) {
      AdjustOverrun();
    }
    // If there is only one fragment in the buffer, signal underrun as well
    if (delay && BufferedSamples < (FragSamples << 1)) {
      GenerateSamples(FragSamples);
      AdjustUnderrun();
    }
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

/// HQSound::AdjustOverrun
// Signal a buffer overrun
void HQSound::AdjustOverrun(void)
{
  LONG newfreq;
  // The buffer is running too full. This means we are
  // generating samples too fast. Reduce the sampling frequency.
  // We must do this very carefully as overruns accumulate data
  newfreq = (EffectiveFreq * 8191) >> 13;
  if (newfreq >= EffectiveFreq)
    newfreq--;
  EffectiveFreq = newfreq;
  // Drop buffer bytes we should have generated so far.
  UpdateSamples = 0;
  //printf("Overrun! Freq now=%ld\n",EffectiveFreq);
}
///

/// HQSound::AdjustUnderrun
// Signal a buffer underrun
void HQSound::AdjustUnderrun(void)
{
  LONG newfreq;
  // The buffer is running empty. We are generating
  // too few samples. Enlarge the effective frequency to keep track.
  newfreq = (EffectiveFreq << 12) / 4095;
  if (newfreq <= EffectiveFreq)
    newfreq++;
  if (newfreq <= (SamplingFreq << 1))
    EffectiveFreq = newfreq;
  // We underrrun, or are near to underrun. We'd better update the
  // the buffer and flush all buffered bytes to prevent the worst.
  UpdateBuffer = true;
  //printf("Underrun! Freq now=%ld\n",EffectiveFreq);
}
///

/// HQSound::DisplayStatus
// Display the status of the sound over the monitor
void HQSound::DisplayStatus(class Monitor *mon)
{
#ifdef USE_SOUND
  mon->PrintStatus("Audio Output Status:\n"
		   "\tAudio output enable            : %s\n"
		   "\tConsole speaker enable         : %s\n"
		   "\tConsole speaker volume         : " ATARIPP_LD "\n"
		   "\tAudio output device            : %s\n"
		   "\tSampling frequency             : " ATARIPP_LD "Hz\n"
		   "\tFragment size exponent         : " ATARIPP_LD "\n"
		   "\tNumber of fragments            : " ATARIPP_LD "\n"
		   "\tNumber of samples in the queue : " ATARIPP_LD "\n"
		   "\tEffective sampling frequency   : " ATARIPP_LD "Hz\n"
		   "\tChannel duplication            : %s\n"
		   "\tStereo sound                   : %s\n"
		   "\tChannel bit depth              : %d\n"
		   "\tAudio data is                  : %s\n",
		   (EnableSound)?("on"):("off"),
		   (EnableConsoleSpeaker)?("on"):("off"),
		   ConsoleVolume,
		   DspName,
		   SamplingFreq,
		   FragSize,
		   NumFrags,
		   BufferedSamples,
		   EffectiveFreq,
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

/// HQSound::ParseArgs
// Parse off command line arguments for the sound handler
void HQSound::ParseArgs(class ArgParser *args)
{
#ifdef USE_SOUND
  bool enable = EnableSound;
  
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  
  args->DefineTitle("OssHQSound");
  args->DefineBool("EnableSound","enable audio output",enable);
  args->DefineBool("EnableConsoleSpeaker","enable the console speaker",
		   EnableConsoleSpeaker); 
  args->DefineBool("ForceStereo","enforce stereo output for broken ALSA interfaces",ForceStereo);
  args->DefineLong("ConsoleSpeakerVolume","set volume of the console speaker",
		   0,64,ConsoleVolume);
  args->DefineString("AudioDevice","set audio output device",DspName);
  args->DefineLong("SampleFreq","set audio sampling frequency",
		   4000,48000,SamplingFreq);
  args->DefineLong("FragSize","set the exponent of the fragment size",
		   2,16,FragSize);
  args->DefineLong("NumFrags","specify the number of fragments",
		   6,256,NumFrags);  

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

