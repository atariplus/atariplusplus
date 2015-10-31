/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: osshqsound.hpp,v 1.13 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Os interface towards sound output for the Oss sound system
 ** with somewhat more quality
 **********************************************************************************/

#ifndef OSSHQSOUND_HPP
#define OSSHQSOUND_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "chip.hpp"
#include "timer.hpp"
#include "sound.hpp"
#if HAVE_FCNTL_H && HAVE_SYS_IOCTL_H && HAVE_SYS_SOUNDCARD_H && HAS_EAGAIN_DEFINE
#define USE_SOUND
#endif
///

/// Forwards
class Pokey;
class Machine;
class Monitor;
///

/// Class HQSound
// This class does all the sound output for us.
class HQSound : public Sound {
  //
  // Name of the game: What is the path of the audio device. Should
  // be /dev/dsp
  char            *DspName;
  //
  // last but not least, the stream of where the sound goes into.
  // Under Linux, this is /dev/dsp.
  int              SoundStream;
  //
  // Sound configuration
  //
  // Effective output frequency. We reduce or increase this
  // depending on whether the buffer over- or underruns.
  LONG             EffectiveFreq;
  //
  // frequency carry-over from last computation loop
  LONG             CycleCarry;
  // 
  // Fragment size: This is the size of the DMA buffer in bytes(!)
  // as the exponent of the base two.
  LONG             FragSize;
  //
  // Fragsize in samples
  ULONG            FragSamples;
  //
  // Number of fragments: Number of DMA buffers. Two means double
  // buffering and so on.
  LONG             NumFrags;
  //
  // Total number of bytes in the output buffer
  ULONG            BufferedSamples;
  //
  // Enforce output in stereo, works around bugs in ALSA
  bool             ForceStereo;
  //
  // This bool gets set if we must update the audio buffer
  // because either te device requires more data, or the audio
  // settings got altered.
  bool             UpdateBuffer;
  // The number of bytes we should have generated, but for which we
  // delayed the generation to reduce the overhead.
  ULONG            UpdateSamples;
  //
  // Generate the given number (not in bytes, but in number) of audio samples
  // and place them into the tail of the ready buffer list.
  void GenerateSamples(ULONG numsamples);
  //
  // Feed data into the dsp device by taking buffered bytes from the queue
  // and removing the sample buffers, putting them back into the free list.
  // This returns false on a buffer underrun
  bool FeedDevice(class Timer *delay);
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
public:
  // Constructor and destructor
  HQSound(class Machine *);
  ~HQSound(void);
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
///

///
#endif
  
  
