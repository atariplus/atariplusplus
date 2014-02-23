/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: wavsound.hpp,v 1.16 2013/05/31 16:48:19 thor Exp $
 **
 ** In this module: Os interface for .wav file output
 **********************************************************************************/

#ifndef WAVSOUND_HPP
#define WAVSOUND_HPP

/// Includes
#include "types.hpp"
#include "chip.hpp"
#include "timer.hpp"
#include "sound.hpp"
#include "stdio.hpp"
#include "list.hpp"
#include "string.hpp"
#include "new.hpp"
///

/// Forwards
class Pokey;
class Machine;
class Monitor;
///

/// Class WavSound
// This class does all the sound output for us.
class WavSound : public Sound {
  //
  // Name of the game: What is the output path for the data.
  char            *FileName;
  //
  // Name of the Oss output device
  char            *DspName;
  //
  // last but not least, the stream of where the sound goes into.
  FILE            *SoundStream;
  //
  // The oss playback stream to listen to the output
  int              OssStream;
  //  
  LONG             FragSize;
  LONG             NumFrags;
  LONG             FragSamples;
  //
  // Sound configuration
  //
  // Sound playback is enabled if this is true
  bool             Playback;
  //
  // Enable recording after a reset.
  bool             EnableAfterReset;
  //
  // Force stereo output because mono output is broken on
  // some devices.
  bool             ForceStereo;
  //
  // Wav output characteristics
  bool             WavStereo;     // generate WAV in stereo (both samples identically)
  bool             WavSixteen;    // generate WAV output in 16 bit, must be LE for WAV.
  //
  // Will get set by the first change in the sound output
  // except for the console speaker events.
  bool             Recording;
  bool             HaveMutingValue;
  UBYTE            MutingValue;
  //
  // Keep the number of samples recorded here. This is required to 
  // complete the .wav file header.
  LONG             OutputCounter;
  //
  // Residual from the last division for the computation of the number of
  // samples. This is kept here to fit the overall frequency perfectly
  // on the LONG run. Hence, some kind of "Bresenham" algorithm here.
  int              Residual;
  int              Correction;
  int              BufferSamples;
  //
  // Structure of the .wav file header. This includes all but
  // the body.
  struct WavHeader {
    char           RIFF[4];        // header type. must be RIFF
    char           RIFF_l[4];      // file size in little endian
    char           WAVE[4];        // file type. must be WAVE
    char           fmt_[4];        // format chunk id. Must be fmt_
    char           fmt_l[4];       // size of the of the format chunk in bytes, little endian
    char           ext[2];         // no idea what this is. Must be 0x01,0x00
    char           numch[2];       // number of channels. We only write one
    char           rate[4];        // sample rate in Hz.
    char           bytespsec[4];   // bytes per second
    char           bytespersam[2]; // bytes per sample.
    char           bitspersam[2];  // bits per sample. Here always eight.
    char           data[4];        // data chunk id. Must be data.
    char           data_l[4];      // length of the data chunk in bytes
    //
    // Construct a wav header from the size of the body.
    WavHeader(LONG samples,LONG samplingfreq,bool stereo,bool sixteenbit);
    //
    // Write the header to a file.
    bool WriteHeader(FILE *stream);
  };
  //
  // Setup the sound output buffer specs from
  // the sampling frequency
  void InitializeBuffer(void);
  //
  // Private for Oss-Playback: Open the Oss sound output.
  bool OpenOssStream(void);
  // and close it again.
  void CloseOssStream(void);
  //
  // Private for sound file construction: Write the .wav header.
  // Returns true in case the file opened fine.
  bool OpenWavFile(void);
  //
  // Close the wav file, and complete the header. If the argument is
  // true, then the file is assumed complete and the header is cleaned
  // up. Otherwise, the file is disposed and deleted.
  void CloseWavFile(bool finewrite);
  //
public:
  // Constructor and destructor
  WavSound(class Machine *);
  ~WavSound(void);
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
