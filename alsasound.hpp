/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: alsasound.hpp,v 1.14 2015/03/21 14:31:27 thor Exp $
 **
 ** In this module: Os interface towards sound output for the alsa sound system
 **********************************************************************************/

#ifndef ALSASOUND_HPP
#define ALSASOUND_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "chip.hpp"
#include "timer.hpp"
#include "sound.hpp"
#if HAVE_ALSA_ASOUNDLIB_H && HAVE_SND_PCM_OPEN && HAS_PROPER_ALSA
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#endif
///

/// Forwards
class Pokey;
class Machine;
class Monitor;
///

/// Class AlsaSound
// This class does all the sound output for us.
#if HAVE_ALSA_ASOUNDLIB_H && HAVE_SND_PCM_OPEN && HAS_PROPER_ALSA
class AlsaSound : public Sound {
  //
  // Name of the hardware card we want for output. Should be hw:0,0
  char                *CardName;
  //
  // Where the data goes to.
  snd_pcm_t           *SoundStream;
  //
  // The hardware parameters of the stream.
  snd_pcm_hw_params_t *HWParms;
  snd_pcm_sw_params_t *SWParms;
  //
  // The Alsa Async Event handler that gets called for
  // new samples
  snd_async_handler_t *AsyncHandler;
  //
  // The following bool is set if Pokey is currently not
  // in use and we may run it to get more samples.
  bool                 MayRunPokey;
  //
  // The following bool is set in case the async handler
  // can be run savely.
  volatile bool        AbleIRQ;
  //
  // Effective output frequency. We reduce or increase this
  // depending on whether the buffer over- or underruns.
  LONG                 EffectiveFreq;
  //
  // Differential adjustment (D-part of the control loop)
  LONG                 DifferentialAdjust;
  //
  // frequency carry-over from last computation loop
  LONG                 CycleCarry;
  // 
  // Fragment size: This is the size of the DMA buffer in bytes
  // as the exponent of the base two.
  LONG                 FragSize;
  //
  // Number of fragments: Number of DMA buffers. Two means double
  // buffering and so on.
  LONG                 NumFrags;
  //
  // Total number of samples in the output buffer
  ULONG                BufferedSamples;
  ULONG                BufferSize;      // total size of the buffer in samples
  ULONG                FragSamples;     // samples in a fragment
  //
  // Enforce stereo output even though mono would do.
  bool                 ForceStereo;
  //
  // This bool gets set if we must update the audio buffer
  // because either te device requires more data, or the audio
  // settings got altered.
  bool                 UpdateBuffer;
  //
  // This is set if we have to use polling instead of interrupts.
  bool                 Polling;
  //
  // The number of samples we should have generated, but for which we
  // delayed the generation to reduce the overhead.
  ULONG                UpdateSamples;
  //
  // Generate the given number (not in bytes, but in number) of audio samples
  // and place them into the tail of the ready buffer list.
  void GenerateSamples(ULONG numsamples);
  //
  // ALSA callback that gets invoked whenever enough room is in the
  // buffer. This is just the stub function that loads the "this" pointer.
  static void AlsaCallBackStub(snd_async_handler_t *ahandler);
  //
  // The real callback
  void AlsaCallBack(void);
  //
  // Signal a buffer overrun
  void AdjustOverrun(void);
  //
  // Signal a buffer underrun
  void AdjustUnderrun(void);
  //
  // Private setup of the dsp: Initialize and
  // configure the dsp for the user specified
  // parameters. Returns false in case the dsp cannot be setup.
  bool InitializeDsp(void);
  //
  // Suspend audio playing since we need access to the buffer.
  void SuspendAudio(void);
  //
  // Resume playing, grant buffer access again.
  void ResumeAudio(void);
public:
  // Constructor and destructor
  AlsaSound(class Machine *);
  ~AlsaSound(void);
  //
  // Update the output sound, feed new data into the DSP.
  // Delay by the timer or don't delay at all if no 
  // argument given.
  virtual void UpdateSound(class Timer *delay = NULL);
  //  
  // Let the sound driver know that 1/15Khz seconds passed.
  // This might be required for resynchronization of the
  // sound driver.
  virtual void HBI(void);
  //
  // Turn the console speaker on or off:
  virtual void ConsoleSpeaker(bool);
  //
  // the following are imported by the Chip class:
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Display the current settings of the sound module
  virtual void DisplayStatus(class Monitor *mon);
  // 
  // Parse off arguments relevant for us
  virtual void ParseArgs(class ArgParser *args);
  //
};
#endif
///

///
#endif
  
  
