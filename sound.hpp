/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sound.hpp,v 1.18 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: generic Os interface towards sound output
 **********************************************************************************/

#ifndef SOUND_HPP
#define SOUND_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "chip.hpp"
#include "timer.hpp"
#include "vbiaction.hpp"
#include "hbiaction.hpp"
#include "pokey.hpp"
///

/// Forwards
class Machine;
class Monitor;
struct AudioBufferBase;
///

/// Class Sound
// This class does all the sound output for us.
class Sound : public Chip, public VBIAction, public HBIAction {
protected:
  // The sound output requires support from pokey that does all the sound
  class Pokey            *LeftPokey;
  class Pokey            *RightPokey;
  //
  // The pokey base frequency. Has to be read by the subclasses.
  int                     PokeyFreq;
  //
  // Settings for the buffers (as allocated and filled)
  bool                    SignedSamples;
  bool                    Stereo;
  bool                    SixteenBit;
  bool                    LittleEndian;
  bool                    Interleaved;
  // Sampling frequency for generating the samples
  LONG                    SamplingFreq;
  // State of the console speaker (on or off)
  bool                    ConsoleSpeakerStat;
  // The audio data ring-buffer. The first buffer contains free
  // buffer slots, the second busy (to be played) ones.
  List<AudioBufferBase>   FreeBuffers;
  List<AudioBufferBase>   ReadyBuffers;
  struct AudioBufferBase *PlayingBuffer;
  //  
  //  
  // Generic sound preferences.
  bool                    EnableSound;
  bool                    EnableConsoleSpeaker;  
  LONG                    ConsoleVolume; // volume of the console speaker
  //
  // Generate the given number (not in bytes, but in number) of audio samples
  // and place them into the tail of the ready buffer list.
  // Make buffers at least fragsize samples large. Returns the number of
  // generated samples (frames in the language of ALSA).
  ULONG GenerateSamples(ULONG numsamples,ULONG fragsize);
  //
  // Cleanup the buffer for the next go
  void CleanBuffer(void);
  //
  // On VBI, provided we aren't late, update the sound.
  // This implements the interface of the VBIAction class.
  // This is the one and only class in the VBI chain that finally
  // is allowed to time something.
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
public:
  //
  // Constructor and destructor
  Sound(class Machine *);
  virtual ~Sound(void);
  //
  // Update the output sound, feed new data into the DSP.
  // Delay by the timer or don't delay at all if no 
  // argument given.
  virtual void UpdateSound(class Timer *delay = NULL) = 0;
  //  
  // Let the sound driver know that 1/15Khz seconds passed.
  // This might be required for resynchronization of the
  // sound driver.
  virtual void HBI(void) = 0;
  //
  // Turn the console speaker on or off
  virtual void ConsoleSpeaker(bool onoff) = 0;
  //
  // the following are imported by the Chip class:
  virtual void ColdStart(void) = 0;
  virtual void WarmStart(void) = 0;
  //
  // Display the current settings of the sound module
  virtual void DisplayStatus(class Monitor *mon) = 0;
  // 
  // Parse off arguments relevant for us
  virtual void ParseArgs(class ArgParser *args) = 0;
};
///

///
#endif
  
  
