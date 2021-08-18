/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: alsasound.cpp,v 1.38 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Os interface towards sound output for the alsa sound system
 **********************************************************************************/

/// Includes
#include "types.h"
#include "sound.hpp"
#include "machine.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "timer.hpp"
#include "pokey.hpp"
#include "cpu.hpp"
#include "exceptions.hpp"
#include "alsasound.hpp"
#include "audiobuffer.hpp"
#include "unistd.hpp"
#include "new.hpp"
#include <assert.h>
#if HAVE_ALSA_ASOUNDLIB_H && HAVE_SND_PCM_OPEN && HAS_PROPER_ALSA
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
///

/// Defines
#define ThrowAlsa(err,object,desc)  throw(AtariException(0,snd_strerror(err),object,desc))
///

/// AlsaSound::AlsaSound
AlsaSound::AlsaSound(class Machine *mach)
  : Sound(mach), CardName(new char[8]),
    SoundStream(NULL), HWParms(NULL), SWParms(NULL), AsyncHandler(NULL),
    MayRunPokey(false), AbleIRQ(false),
    FragSize(8), NumFrags(12), 
    BufferedSamples(0), ForceStereo(false), UpdateBuffer(false), Polling(false), 
    UpdateSamples(0)
{
  strcpy(CardName,"default"); //hw:0,0");
}
///

/// AlsaSound::~AlsaSound
AlsaSound::~AlsaSound(void) 
{

  // get rid of all data within here.
  if (SoundStream) {
    // This also unlinks the pcm handler.
    SuspendAudio();
    snd_pcm_close(SoundStream);
    SoundStream = NULL;
  }
  if (HWParms) {
    snd_pcm_hw_params_free(HWParms);
    HWParms = NULL;
  }
  if (SWParms) {
    snd_pcm_sw_params_free(SWParms);
    SWParms = NULL;
  }
  delete[] CardName;
  CleanBuffer();
}
///

/// AlsaSound::SuspendAudio
// Suspend audio playing since we need access to the buffer.
void AlsaSound::SuspendAudio(void)
{
  // Stop processing of the signal handler
  // we need its data structures.
  AbleIRQ = false;
}
///

/// AlsaSound::ResumeAudio
// Resume playing, grant buffer access again.
void AlsaSound::ResumeAudio(void)
{
  if (SoundStream) {    
    // Push a couple of samples into the buffer.
    // Check whether we have an underrun here. If so, switch back to preparation state
    if (snd_pcm_state(SoundStream) == SND_PCM_STATE_XRUN) {
      // Outch, underrun.
      snd_pcm_prepare(SoundStream);
      //printf("ResumeAudio based ");
      AdjustUnderrun();
    }
    // Due to the start threshold handling, this should run the PCM automatically.
    AbleIRQ     = false;
    MayRunPokey = true;
    AlsaCallBack();
    MayRunPokey = false;
    AbleIRQ     = true;
  }
}
///

/// AlsaSound::GenerateSamples
// Generate the given number (not in bytes, but in number) of audio samples
// and place them into the tail of the ready buffer list.
void AlsaSound::GenerateSamples(ULONG numsamples)
{	
  // Let the sound class do the work and update the sample count.
  BufferedSamples += Sound::GenerateSamples(numsamples,FragSamples);
}
///

/// AlsaSound::ColdStart
// Run a coldstart. Also initializes the dsp device if we haven't done so
// before. Or at least, it tries to.
void AlsaSound::ColdStart(void)
{
  LeftPokey  = machine->Pokey(0);
  RightPokey = machine->Pokey(1);
  // Check whether the user requested output. If so, try to configure the
  // dsp for output.
  if (EnableSound && SoundStream == NULL) {
    if (!InitializeDsp()) {
      // opening or configuring /dev/dsp failed. Do not try again!
      EnableSound = false;
    }
  }
  //
  // Now run for the warmstart.
  WarmStart();
}
///

/// AlsaSound::WarmStart
// Run a simple reset here.
void AlsaSound::WarmStart(void)
{
  ConsoleSpeakerStat = false;  
  //
  // Dispose the old audio buffer nodes now.
  CleanBuffer();
  //
  // Reset generation frequency
  EffectiveFreq      = SamplingFreq;
  DifferentialAdjust = 0;
  BufferedSamples    = 0;
}
///

/// AlsaSound::ConsoleSpeaker
// Turn the console speaker on or off:
void AlsaSound::ConsoleSpeaker(bool onoff)
{
  if (ConsoleSpeakerStat != onoff) {
    ConsoleSpeakerStat = onoff;
    //UpdateBuffer = true;
    UpdateSound(NULL);
  }
}
///

/// AlsaSound::InitializeDsp
// Opens the sound device /dev/dsp and installs all
// user parameters here. Call this only if the sound
// is enabled.
// Return true on success, false on error.
bool AlsaSound::InitializeDsp(void)
{
  int dir,err;
  unsigned int rrate,channels;
  snd_pcm_format_t format;
  snd_pcm_uframes_t fragsize;
  //
  // Allocate hardware parameter structure.
  if (HWParms == NULL) {
    if ((err = snd_pcm_hw_params_malloc(&HWParms)) < 0) {
      ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to allocate hardware parameter information");
    }
  }
  if (SWParms == NULL) {
    if ((err = snd_pcm_sw_params_malloc(&SWParms)) < 0) {
      ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to allocate software parameter information");
    }
  }
  //
  // Open the sound stream.
  if (SoundStream == NULL) {
    // o.k., the stream is not yet open. Do now.
    if ((err = snd_pcm_open(&SoundStream,CardName,SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK | SND_PCM_ASYNC)) < 0) {
      machine->PutWarning("Audio Setup:\n"
			  "Couldn't open %s for audio output, disabling it for now.\n"
			  "For the next time, either make %s available or disable the\n"
			  "sound output.\nFailure: %s\n",
			  CardName,CardName,snd_strerror(err));
      return false;
    }
  }
  //
  // Initialize hardware parameters with full configuration space
  if ((err = snd_pcm_hw_params_any(SoundStream,HWParms)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","Unable to configure the audio card");
  }
  //
  // Set interleaved access mode, i.e. write channels one after another
  // We will combine this with async notification
  if ((err = snd_pcm_hw_params_set_access(SoundStream,HWParms,SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to provide interleaved sample access");
  }
  //
  // Set the number of channels for the audio output. We only need mono.
  // The number of channels could be either one or two. This depends on whether
  // we want to generate sound by a second pokey.
  channels = RightPokey?2:1;
  //
  // Some buggy alsa implementations do not work unless I force them to
  // stereo, even though mono should do fine.
  if (ForceStereo)
    channels = 2;
  //
  if ((err = snd_pcm_hw_params_set_channels_min(SoundStream,HWParms,&channels)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to set the minimum channel count");
  }
  channels = 2;
  if ((err = snd_pcm_hw_params_set_channels_max(SoundStream,HWParms,&channels)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to set the minimum channel count");
  }
  channels = 0;
  if ((err = snd_pcm_hw_params_set_channels_first(SoundStream,HWParms,&channels)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to restrict to the minimum channel count");
  }
  // Set channel format to interleaved if there is a choice.
  if ((err = snd_pcm_hw_params_set_access(SoundStream,HWParms,SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to set the sample format layout");
  }
  //
  // Set DSP sample rate and read it back: Problem: The documentation does not fit the
  // header file here.
  dir          = 0;
  rrate        = SamplingFreq;
  if ((err = snd_pcm_hw_params_set_rate_near(SoundStream,HWParms,&rrate,&dir)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","failed to setup the sampling rate");
  }
  SamplingFreq = rrate;
  //
  // Setup the size of the buffer.
  fragsize     = NumFrags<<FragSize;
  if ((err = snd_pcm_hw_params_set_buffer_size_near(SoundStream,HWParms,&fragsize)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","failed to setup the buffer size");
  }
  //
  // Setup the size of a fragment.
  fragsize     = 1<<FragSize;
  if ((err = snd_pcm_hw_params_set_period_size_near(SoundStream,HWParms,&fragsize,&dir)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","failed to setup the fragment size");
  }
  //
  // Narrow the configuration space for formats.
  // Note: Apparently, some alsa backends do not support narrowing.
  // Just ignore the error then...
  format = SND_PCM_FORMAT_S8;
  snd_pcm_hw_params_set_format_first(SoundStream,HWParms,&format);
  format = SND_PCM_FORMAT_U16_BE;
  snd_pcm_hw_params_set_format_last(SoundStream,HWParms,&format);
  //
  // Now query the format. We support quite some, but not all formats.
  // What happens if more than one format is available?
  if ((err = snd_pcm_hw_params_get_format(HWParms,&format)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to read the present hardware format");
  }
  switch(format) {
  case SND_PCM_FORMAT_U8:
    SignedSamples = false;
    SixteenBit    = false;
    LittleEndian  = false;
    break;
  case SND_PCM_FORMAT_S8:
    SignedSamples = true;
    SixteenBit    = false;
    LittleEndian  = false;
    break;
  case SND_PCM_FORMAT_S16_LE: // signed 16 bit little endian
    SignedSamples = true;
    SixteenBit    = true;
    LittleEndian  = true;
    break;
  case SND_PCM_FORMAT_S16_BE: // signed 16 bit big endian
    SignedSamples = true;
    SixteenBit    = true;
    LittleEndian  = false;
    break;
  case SND_PCM_FORMAT_U16_LE: // unsigned 16 bit little endian
    SignedSamples = false;
    SixteenBit    = true;
    LittleEndian  = true;
    break;
  case SND_PCM_FORMAT_U16_BE: // unsigned 16 bit bit endian
    SignedSamples = false;
    SixteenBit    = true;
    LittleEndian  = false;
    break;
  default:
    Throw(InvalidParameter,"AlsaSound::InitializeDsp","Unknown audio sample format");
  }
  //
  // Read back. We may also provide stereo
  if ((err = snd_pcm_hw_params_get_channels(HWParms,&channels)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to query the number of channels");
  }
  switch(channels) {
  case 2:
    if (RightPokey) {
      Stereo      = false; // this is actually a channel-duplication flag
      Interleaved = true;
    } else {
      Stereo      = true; // only stereo output supported
      Interleaved = false;
    }
    fragsize    <<= 1;    // convert from bytes to samples
    break;
  case 1:
    Stereo        = false;
    Interleaved   = false;
    break;
  default:
    Throw(InvalidParameter,"AlsaSound::InitializeDsp","Unsupported number of channels");
    break;
  }
  //
  // Hardware setup done. Now write the data back into the device.
  // Apparently, this may also fail on some ^$?$!! devices.
  snd_pcm_hw_params(SoundStream,HWParms);
  //
  // Setup the software buffering here.
  //
  // Get the current software parameters here.
  if ((err = snd_pcm_sw_params_current(SoundStream,SWParms)) >= 0) {
    //
    // Start the playback if the buffer is almost full.
    if ((err = snd_pcm_sw_params_set_start_threshold(SoundStream,SWParms,(NumFrags - 2)<<FragSize)) < 0) {
      ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to set the playback start threshold");
    }
    //
    // Set the wakeup point: Signal an error if less than this is available.
    if ((err = snd_pcm_sw_params_set_avail_min(SoundStream,SWParms,1<<FragSize)) < 0) {
      ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to set the wakeup point");
    }
    //
    /*
    ** This is deprecated, and not even needed
    // Align all transfers to one sample. I've no idea why this is useful.
    if ((err = snd_pcm_sw_params_set_xfer_align(SoundStream,SWParms,1)) < 0) {
    ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to set the transfer align to one");
    }
    **
    */
    //
    // Write the parameters to the playback device now.
    if ((err = snd_pcm_sw_params(SoundStream,SWParms)) < 0) {
      ThrowAlsa(err,"AlsaSound::InitializeDsp","unable to write back the software parameters");
    }
  }
  //
  // Setup the effective buffering frequency.
  EffectiveFreq      = SamplingFreq;
  DifferentialAdjust = 0;
  CycleCarry         = 0;
  UpdateBuffer       = false;
  Polling            = false;
  UpdateSamples      = 0;
  // Compute the size of a frame in bits
  FragSamples        = fragsize;
  BufferSize         = FragSamples * NumFrags;
  //
  // Start the async handler now.
  if ((err = snd_async_add_pcm_handler(&AsyncHandler,SoundStream,&AlsaSound::AlsaCallBackStub,this)) < 0) {
    Polling = true; 
    // Sigh, so go into the polling mode. Some ^#+$!@!!
    // sound preventers/handlers like pulse do not support the
    // full sound API.
  }
  //
  // Start the sound processing now.
  // NO! Can't do that, pokey is not yet initialized!
  // ResumeAudio();
  //
  return true;
}
///

/// AlsaSound::AlsaCallBackStub
// ALSA callback that gets invoked whenever enough room is in the
// buffer. This is just the stub function that loads the "this" pointer.
void AlsaSound::AlsaCallBackStub(snd_async_handler_t *ahandler)
{        
  class AlsaSound *that = (class AlsaSound *)snd_async_handler_get_callback_private(ahandler);

  if (that->AbleIRQ) {
    // Run the real callback now if we may.
    that->AlsaCallBack();
    //
    // The ResumeIRQ will generate samples for us in case we miss this
    // interrupt.
  }
}
///

/// AlsaSound::AlsaCallBack
// The real callback that gets invoked whenever new sound data requires computation
void AlsaSound::AlsaCallBack(void)
{
  snd_pcm_sframes_t avail = 0;
  
  if (EnableSound && SoundStream) {
    // Get the number of available frames in the output buffer.
    avail = snd_pcm_avail_update(SoundStream);
    while(avail >= (1L << FragSize)) {
      // Round down to integers of the fragment size.
      avail &= -(1L << FragSize);
      // Get the next buffer we want to play back.
      if (PlayingBuffer == NULL) {
	struct AudioBufferBase *next = ReadyBuffers.First();
	// No playing buffer. Pull a new one from the list of ready buffers.
	if (next && next->FreeSamples() == 0) {
	  next->Remove();
	  PlayingBuffer = next;
	}
      }
      if (PlayingBuffer == NULL) {
	// Ok, check whether we may launch pokey directly here. This might
	// be valid if the main thread is waiting in the VBI anyhow.	   
	//printf("AlsaCallBack based ");
	AdjustUnderrun();
	if (MayRunPokey) {
	  //printf("Alsa buffer run out of data, must generate more now\n");
	  GenerateSamples(avail);
	  continue;
	} else {
	  // Unfortunately, we cannot directly call pokey
	  // here since we don't know the state of it and whether something
	  // else is currently playing with it.
	  return;
	}
      }
      if (PlayingBuffer) {
	int cpy,err;
	// We do have a buffer now. Ok, copy as many bytes as required into the output buffer.
	cpy = PlayingBuffer->ReadySamples();
	if (cpy > avail)
	  cpy = avail;
	//
	// Write the indicated number of bytes into the stream if we can.
	err = snd_pcm_writei(SoundStream,PlayingBuffer->ReadPtr,cpy);
	// If we have an error, ignore this frame and continue.
	if (err < 0) {      
	  //printf("underrun %d\n",err);
	  return;
	}
#if 0
	{
	  static ULONG samplecnt = 0;
	  static time_t starttime = 0;
	  static time_t nowtime = 0;
	  struct timeval tv;

	  if (starttime == 0) {
	    gettimeofday(&tv,NULL);
	    starttime = tv.tv_sec;
	  }
	  gettimeofday(&tv,NULL);
	  nowtime = tv.tv_sec;

	  if (nowtime> starttime) {
	    printf("%g\n",samplecnt / (double)((nowtime - starttime)));
	  }
	  samplecnt += err;
	}
#endif
	//printf("%d.%d.%d\n",avail,cpy,err);
	// Otherwise, it returns the number of frames (does it?)
	avail                  -= err;
	BufferedSamples        -= err;
	// Adjust the playing buffer pointers now.
	PlayingBuffer->ReadPtr += err << PlayingBuffer->SampleShift;
	//
	// Check whether we have a playing buffer that is empty. If so, put it
	// back if we can.
	if (PlayingBuffer->IsEmpty()) {	  
	  // We're done with it. Dispose it.
	  FreeBuffers.AddTail(PlayingBuffer);
	  PlayingBuffer = NULL;
	}
      }
    }
    /*
    if (BufferedSamples < FragSamples) {
      // Better enlarge the frequency to avoid the trouble...
      printf("AlsaCallBack tail based ");
      AdjustUnderrun();
    }
    */
  }
}
///

/// AlsaSound::HBI
// Let the sound driver know that 1/15Khz seconds passed.
// This might be required for resynchronization of the
// sound driver.
void AlsaSound::HBI(void)
{
  
  if (EnableSound && Polling) {
    AlsaCallBack();
  }
  
  if (EnableSound) {
    LONG  remaining,samples;
    ULONG cycles   = machine->CPU()->ElapsedCycles();
    // Compute the number of samples we need to generate this time.
    // Note that the pokey base frequency is the HBI frequency. The number of clocks
    // per HBI is 114.
    remaining      = EffectiveFreq + DifferentialAdjust; // the number of sampling cycles left this time.
    // number of samples to generate this time.
    samples        = (remaining * cycles + CycleCarry) / (PokeyFreq * 114);         
    // keep the number of samples we did not take due to round-off   
    CycleCarry    += remaining  * cycles - samples * PokeyFreq * UQUAD(114);
    assert(CycleCarry >= 0);
    UpdateSamples += samples;
    //
    if (UpdateSamples > 0) {
      // Compute this number of samples, and put it into the buffer.
      SuspendAudio();
      GenerateSamples(UpdateSamples);
      ResumeAudio();
      UpdateSamples     = 0;
      UpdateBuffer      = false;
    }
  }
}
///

/// AlsaSound::UpdateSound
// Update the output sound, feed new data into the DSP.
// Delay by the timer or don't delay at all if no 
// argument given.
void AlsaSound::UpdateSound(class Timer *delay)
{
  if (EnableSound) {
    // Signal that we must update the sound now since the pokey parameters changed.
    UpdateBuffer = true;
    if (delay) {    
      // Here we are in the VBI state. Check now at the end of the VBI
      // how many bytes are left.
      SuspendAudio();
      DifferentialAdjust = 0;
      // Check how many bytes we got buffered. If too many, cut the frequency down.
      if (BufferedSamples > (BufferSize + FragSamples)) {
	AdjustOverrun();
      }
      ResumeAudio();
      //
      MayRunPokey       = true; // as no one is working with it, go ahead!
      delay->WaitForEvent();    
      MayRunPokey       = false;
      //
      SuspendAudio();
      if (BufferedSamples < (FragSamples << 2)) {
	// Better enlarge the frequency to avoid the trouble...
	//printf("UpdateSound based ");
	GenerateSamples((FragSamples << 2) - BufferedSamples);
	AdjustUnderrun();
      }
      ResumeAudio();
    } else {
      HBI();
    }
  } else {
    // No sound enabled, just wait.
    if (delay)
      delay->WaitForEvent();
  }
}
///

/// AlsaSound::AdjustOverrun
// Signal a buffer overrun
void AlsaSound::AdjustOverrun(void)
{
  LONG newfreq;
  // The buffer is running too full. This means we are
  // generating samples too fast. Reduce the sampling frequency.
  // We must do this very carefully as overruns accumulate data
  newfreq = (EffectiveFreq * 4095) >> 12;
  if (newfreq >= EffectiveFreq)
    newfreq--;
  EffectiveFreq      = newfreq;
  DifferentialAdjust = -(LONG(BufferedSamples - BufferSize) * newfreq) >> 13;
  if (-DifferentialAdjust >= (newfreq >> 1))
    DifferentialAdjust = -(newfreq >> 1);
  // Drop buffer bytes we should have generated so far.
  UpdateSamples = 0;
#if 0
  printf("Overrun!  BufBytes = %u Freq now=%d, DA= %d, Target = %d\n",
	 BufferedSamples,EffectiveFreq + DifferentialAdjust,DifferentialAdjust,BufferSize);
#endif
}
///

/// AlsaSound::AdjustUnderrun
// Signal a buffer underrun
void AlsaSound::AdjustUnderrun(void)
{
  LONG newfreq;
  // The buffer is running empty. We are generating
  // too few samples. Enlarge the effective frequency to keep track.
  newfreq = (EffectiveFreq << 12) / 4095;
  if (newfreq <= EffectiveFreq)
    newfreq++;
  if (newfreq <= (SamplingFreq << 1))
    EffectiveFreq    = newfreq;
  //
  DifferentialAdjust = 0;
  // We underrrun, or are near to underrun. We'd better update the
  // the buffer and flush all buffered bytes to prevent the worst.
  UpdateBuffer = true;
#if 0
  printf("Underrun! BufBytes = %u Freq now=%d, DA = %d, Target = %d\n",
	 BufferedSamples,EffectiveFreq + DifferentialAdjust,DifferentialAdjust,BufferSize);
#endif
}
///

/// AlsaSound::DisplayStatus
// Display the status of the sound over the monitor
void AlsaSound::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Audio Output Status:\n"
		   "\tAudio output enable           : %s\n"
		   "\tConsole speaker enable        : %s\n"
		   "\tConsole speaker volume        : " ATARIPP_LD "\n"
		   "\tAudio output card             : %s\n"
		   "\tSampling frequency            : " ATARIPP_LD "Hz\n"
		   "\tFragment size exponent        : " ATARIPP_LD "\n"
		   "\tNumber of fragments           : " ATARIPP_LD "\n"
		   "\tNumber of frames in the queue : " ATARIPP_LD "\n"
		   "\tEffective sampling frequency  : " ATARIPP_LD "Hz\n"
		   "\tChannel duplication           : %s\n"
		   "\tStereo sound                  : %s\n"
		   "\tChannel bit depth             : %d\n"
		   "\tAudio data is                 : %s\n",
		   (EnableSound)?("on"):("off"),
		   (EnableConsoleSpeaker)?("on"):("off"),
		   ConsoleVolume,
		   CardName,
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
}
///

/// AlsaSound::ParseArgs
// Parse off command line arguments for the sound handler
void AlsaSound::ParseArgs(class ArgParser *args)
{
  bool enable = EnableSound;
  
  LeftPokey   = machine->Pokey(0);
  RightPokey  = machine->Pokey(1);
  
  args->DefineTitle("AlsaSound");
  args->DefineBool("EnableSound","enable audio output",enable);
  args->DefineBool("EnableConsoleSpeaker","enable the console speaker",
		   EnableConsoleSpeaker);
  args->DefineBool("ForceStereo","enforce stereo output for broken ALSA interfaces",ForceStereo);
  args->DefineLong("ConsoleSpeakerVolume","set volume of the console speaker",
		   0,64,ConsoleVolume);
  args->DefineString("AudioCard","set audio output card",CardName);
  args->DefineLong("SampleFreq","set audio sampling frequency",
		   4000,48000,SamplingFreq);
  args->DefineLong("FragSize","set the exponent of the fragment size",
		   2,16,FragSize);
  args->DefineLong("NumFrags","specify the number of fragments",
		   4,16,NumFrags);  
  // Re-read the base frequency
  PokeyFreq = LeftPokey->BaseFrequency();
  if (SoundStream) {
    SuspendAudio();
    snd_pcm_close(SoundStream);
    SoundStream = NULL;
    CleanBuffer();
    BufferedSamples = 0;
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
}
///

///
#endif
