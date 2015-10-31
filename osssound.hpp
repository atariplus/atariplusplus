/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: osssound.hpp,v 1.16 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Os interface towards sound output for the Oss sound system
 **********************************************************************************/

#ifndef OSSSOUND_HPP
#define OSSSOUND_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "chip.hpp"
#include "timer.hpp"
#include "sound.hpp"
#include "audiobuffer.hpp"
#if HAVE_FCNTL_H && HAVE_SYS_IOCTL_H && HAVE_SYS_SOUNDCARD_H && HAS_EAGAIN_DEFINE
#define USE_SOUND
#endif
///

/// Forwards
class Pokey;
class Machine;
class Monitor;
///

/// Class OssSound
// This class does all the sound output for us.
class OssSound : public Sound {
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
  // Sample frequency divisor: This controls the
  // accuracy of the sample generation process.
  // The higher, the shorter the samples get but
  // the higher the CPU load gets.
  LONG             Divisor;
  //
  // Fragment size: This is the size of the DMA buffer in bytes
  // as the exponent of the base two.
  LONG             FragSize;
  //
  // Number of fragments: Number of DMA buffers. Two means double
  // buffering and so on.
  LONG             NumFrags;
  //
  // Size of the buffer in samples.
  LONG             BufferSize;
  //
  // Enforce output in stereo, works around bugs in ALSA
  bool             ForceStereo;
  //
  // Private setup of the dsp: Initialize and
  // configure the dsp for the user specified
  // parameters. Returns false in case the dsp cannot be setup.
  bool InitializeDsp(void);
  //
public:
  // Constructor and destructor
  OssSound(class Machine *);
  ~OssSound(void);
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
  
  
