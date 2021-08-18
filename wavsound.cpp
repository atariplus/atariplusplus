/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: wavsound.cpp,v 1.33 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Os interface for .wav file output
 **********************************************************************************/

/// Includes
#include "types.h"
#include "new.hpp"
#include "sound.hpp"
#include "machine.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "timer.hpp"
#include "pokey.hpp"
#include "exceptions.hpp"
#include "wavsound.hpp"
#include "audiobuffer.hpp"
#include "unistd.hpp"
#if HAVE_FCNTL_H && HAVE_SYS_IOCTL_H && HAVE_SYS_SOUNDCARD_H
#define USE_PLAYBACK
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#endif
#include <errno.h>
///

/// WavSound::WavSound
WavSound::WavSound(class Machine *mach)
  : Sound(mach), FileName(new char[8]), DspName(new char[9]),
    SoundStream(NULL), OssStream(-1),
    FragSize(9), NumFrags(4), 
    Playback(true), EnableAfterReset(true), ForceStereo(false),
    WavStereo(false), WavSixteen(false),
    Recording(false), HaveMutingValue(true),
    OutputCounter(0)
{
#ifndef USE_PLAYBACK
  Playback = false;
#endif
  EnableSound = true;
  strcpy(FileName,"out.wav");
  strcpy(DspName,"/dev/dsp");
}
///

/// WavSound::~WavSound
WavSound::~WavSound(void) 
{
  // get rid of all data within here.
  CloseWavFile(true);
  // Should we possibly get rid of the file if this is not yet
  // wanted.
  delete[] FileName;
  delete[] DspName;
  //
  CloseOssStream();
  //
  // The audio buffers are disposed by the parent class.
}
///

/// WavSound::ColdStart
// Run a coldstart. This re-initializes recording.
void WavSound::ColdStart(void)
{
  LeftPokey    = machine->Pokey(0);
  RightPokey   = machine->Pokey(1);
  // Check whether the user requested output. If so, try to configure the
  // output file.
  CloseWavFile(false); // dispose it
  //
  // Now run for the warmstart.
  WarmStart();
}
///

/// WavSound::WarmStart
// Run a simple reset here.
void WavSound::WarmStart(void)
{
  ConsoleSpeakerStat = false;
  // dispose the sound nodes queued so far
  CleanBuffer();
  // Now re-initialize the buffer
  // since we disposed it above.
  if (Playback || EnableSound) {
    InitializeBuffer();
  }
}
///

/// WavSound::ConsoleSpeaker
// Turn the console speaker on or off:
void WavSound::ConsoleSpeaker(bool onoff)
{
  // We just keep the statistics, nothing else.
  // Sound is updated every horizontal blank
  ConsoleSpeakerStat = onoff;
}
///

/// WavSound::WavHeader::WavHeader
// Build up a .wav header file from the number of samples and the
// sampling frequency
WavSound::WavHeader::WavHeader(LONG samples,LONG samplingfreq,bool stereo,bool sixteen)
{
  char stereoshift  = char((stereo)?(1):(0));
  char sixteenshift = char((sixteen)?(1):(0));
  LONG hsiz;
  //
  samples <<= stereoshift   + sixteenshift;
  hsiz      = sizeof(*this) + samples - 8; // amount of bytes in the header
  // We fill in the header chunk id.
  RIFF[0]   = 'R';
  RIFF[1]   = 'I';
  RIFF[2]   = 'F';
  RIFF[3]   = 'F';
  // Fill in size of the header including this structure.
  RIFF_l[0] = UBYTE(hsiz);
  RIFF_l[1] = UBYTE(hsiz >> 8);
  RIFF_l[2] = UBYTE(hsiz >> 16);
  RIFF_l[3] = UBYTE(hsiz >> 24);
  WAVE[0]   = 'W';
  WAVE[1]   = 'A';
  WAVE[2]   = 'V';
  WAVE[3]   = 'E';
  // format chunk.
  fmt_[0]   = 'f';
  fmt_[1]   = 'm';
  fmt_[2]   = 't';
  fmt_[3]   = ' ';
  fmt_l[0]  = 0x10; // size of the format chunk: 16 bytes
  fmt_l[1]  = 0x00;
  fmt_l[2]  = 0x00;
  fmt_l[3]  = 0x00;
  ext[0]    = 0x01; // I don't know what this means
  ext[1]    = 0x00;
  numch[0]  = UBYTE((stereo)?(2):(1)); // number of channels
  numch[1]  = 0x00;
  // Sample rate and various other objects
  rate[0]         = UBYTE(samplingfreq);
  rate[1]         = UBYTE(samplingfreq >> 8);
  rate[2]         = UBYTE(samplingfreq >> 16);
  rate[3]         = UBYTE(samplingfreq >> 24);
  samplingfreq  <<= (stereoshift + sixteenshift);
  bytespsec[0]    = UBYTE(samplingfreq);
  bytespsec[1]    = UBYTE(samplingfreq >> 8);
  bytespsec[2]    = UBYTE(samplingfreq >> 16);
  bytespsec[3]    = UBYTE(samplingfreq >> 24);
  bytespersam[0]  = UBYTE(1 << (stereoshift + sixteenshift));  // bytes per sample
  bytespersam[1]  = 0x00;
  bitspersam[0]   = UBYTE((sixteen)?(16):(8)); // bits per sample
  bitspersam[1]   = 0x00;
  // fill in the header for the data chunk now
  data[0]         = 'd';
  data[1]         = 'a';
  data[2]         = 't';
  data[3]         = 'a';
  data_l[0]       = UBYTE(samples);
  data_l[1]       = UBYTE(samples >> 8);
  data_l[2]       = UBYTE(samples >> 16);
  data_l[3]       = UBYTE(samples >> 24);
}
///

/// WavSound::WavHeader::WriteHeader
// Write the wav header to a file. Since we all
// byte-oriented it, we can simply write it to a file directly.
bool WavSound::WavHeader::WriteHeader(FILE *stream)
{
  // We must write exactly one member of this
  if (fwrite(this,sizeof(*this),1,stream) != 1) {
    return false;
  } else {
    return true;
  }
}
///

/// WavSound::OpenOssStream
// Private for Oss-Playback: Open the Oss sound output.
bool WavSound::OpenOssStream(void)
{
#ifdef USE_PLAYBACK
  unsigned int formats,fragsize;
  int channels;
  int freq,dspfreq;
  //
  if (OssStream < 0) {
    // o.k., the stream is not yet open. Do now.
    OssStream = open(DspName,O_WRONLY | O_NONBLOCK,0777);
    //
    // If this didn't work out, print a warning message for the
    // user.
    if (OssStream < 0) {
      machine->PutWarning("Audio Setup:\n"
			  "Couldn't open %s for audio output, disabling it for now.\n"
			  "For the next time, either make %s available or disable the\n"
			  "sound output by \"-Playback false\": %s\n",
			  DspName,DspName,strerror(errno));
      return false;
    }
  }
  //  
  // Setup the fragment size. This is accoring to the Oss manuals the
  // very first thing we should do.
  fragsize = (NumFrags<<16)|FragSize; 
  if (ioctl(OssStream,SNDCTL_DSP_SETFRAGMENT,&fragsize) < 0) {
    ThrowIo("WavSound::InitializeDsp","Cannot setup the fragment specification");
  }
  //
  // Now initialize us by IoCtrl. Set the current audio format, try
  // unsigned char first. These may fail as LONG as the result is
  // fine when quering the state.
  //
  formats = AFMT_U8;
  if (ioctl(OssStream,SNDCTL_DSP_SETFMT, &formats) < 0) {
    machine->PutWarning("Audio Setup:\n"
			"The audio device is unable to support eight bit unsigned sample output required "
			"for .wav playback, disabling playback for now.\n"
			"For the next time, disable it manually by \"-Playback false\".\n");
    close(OssStream);
    OssStream = -1;
    return false;
  }
  //
  // Now query the current format
  //
  formats = AFMT_QUERY;
  if (ioctl(OssStream,SNDCTL_DSP_SETFMT, &formats) < 0) {
    ThrowIo("WavSound::InitializeDsp","Cannot query the output sample format.");
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
    Throw(InvalidParameter,"WavSound::InitializeDsp","Unknown audio sample format");
  }
  //
  // Set the number of channels for the audio output. We only need mono or stereo.
  channels = (RightPokey)?1:0; 
  if (ForceStereo)
    channels = 1;
  if (ioctl(OssStream,SNDCTL_DSP_STEREO,&channels) < 0) {
    ThrowIo("WavSound::InitializeDsp","Cannot set the audio output to mono");
  }  
  switch (channels) {
  case 1:
    if (RightPokey) {
      Stereo      = false; // this is only a channel duplication flag.
      Interleaved = true;
    } else {
      Stereo      = true; // only stereo output supported
      Interleaved = false;
    }
    FragSamples   = (1<<FragSize)>>1;
    break;
  case 0:
    Stereo        = false;
    Interleaved   = false;
    FragSamples   = 1<<FragSize;
    break;
  default:
    Throw(InvalidParameter,"WavSound::InitializeDsp","Unsupported number of channels");
    break;
  }
  //
  // Set the DSP sample rate to the user preferred value.
  // This is optional as LONG as we can read the rate back.
  //
  // We use a sampling frequency that is slightly lower than the frequency of
  // the generated samples. This allows the generator to "catch up" in case
  // we have a high system load and the emulator cannot run at full speed.
  freq = (SamplingFreq * 63) >> 6;
  if (freq == SamplingFreq)
    freq--;
  dspfreq = freq;
  if (ioctl(OssStream,SNDCTL_DSP_SPEED,&dspfreq) < 0) {
    ThrowIo("WavSound::InitializeDsp","Cannot set the audio sampling rate");
  }
  //
  // Read the sampling frequency back. Some dsp's only support a fixed rate, e.g. 48Khz.
  if (ioctl(OssStream,SOUND_PCM_READ_RATE, &dspfreq) < 0) {
    ThrowIo("WavSound::InitializeDsp","Cannot figure out the audio sampling rate");
  }
  // Check whether these two coincide. If not, we have to do something about it
  // and instead alter the frequency by which we generate the samples and write them into
  // the .wav file.
  if (dspfreq != freq) {
    LONG outputfreq;
    outputfreq = (dspfreq << 6) / 63;
    machine->PutWarning("Due to a limitation of your audio hardware,\n"
			"I annot set the WAV output frequency to the\n"
			"desired " ATARIPP_LD " Hz but are using now " ATARIPP_LD " Hz instead.\n"
			"To avoid this feature, disable audio playback.\n",SamplingFreq,outputfreq);
    SamplingFreq = outputfreq;
  }
  // Some devices seem to need a "kick-off" to run the sample generation, as otherwise their
  // select() always returns "non-ready".
  write(OssStream,NULL,0);
  //
  return true;
#else
  return false;
#endif
}
///

/// WavSound::CloseOssStream
// Dispose the sound stream, dispose the audio buffers
void WavSound::CloseOssStream(void)
{  
  // Close the Oss stream if it is open
#ifdef USE_PLAYBACK
  if (OssStream >= 0) {
    close(OssStream);
    OssStream = -1;
  }
#endif
  //
  // dispose the sound nodes queued so far
  CleanBuffer();
}
///

/// WavSound::InitializeBuffer
// Setup the sound output buffer specs from
// the sampling frequency
void WavSound::InitializeBuffer(void)
{
  LONG bufsize;
  bool stereo      = false;
  bool interleaved = false;
  // Compute how many samples we compute per 15.7 Khz. This is given by the ratio of
  // our sampling frequency to the fixed cycle frequency. Round this up to one byte
  // to compute the buffer size.
  delete PlayingBuffer;
  PlayingBuffer   = NULL;
  bufsize         = (SamplingFreq + PokeyFreq - 1) / PokeyFreq;
  if (bufsize < 1) bufsize = 1; // That should not happen, though.
  // Build a new playing buffer to fit the needs of the wav output stream.
  // 16 bit sampling requires signed samples.
  // We set the "stereo" flag whenever we have a single pokey but need/want to
  // generate stereo output. Wav is always little endian.
  if (RightPokey == NULL && WavStereo) {
    stereo = true;
  }
  // We generate interleaved output in case stereo is requested and a second
  // pokey is available.
  if (RightPokey && WavStereo) {
    interleaved = true;
  }
  PlayingBuffer   = AudioBufferBase::NewBuffer(WavSixteen,stereo,WavSixteen,true,interleaved);
  PlayingBuffer->Realloc(bufsize);
  Residual        = 0;
  OutputCounter   = 0;
  MutingValue     = 128;
  BufferSamples   = SamplingFreq / PokeyFreq;
  Correction      = SamplingFreq % PokeyFreq;
  Recording       = false;
  HaveMutingValue = false;
  //
}
///

/// WavSound::OpenWavFile
// Write the .wav file header and setup the .wav
// file as much as possible.
// Return true on success, false on error.
bool WavSound::OpenWavFile(void)
{
  struct WavHeader wh(0,SamplingFreq,WavStereo,WavSixteen);
  // 
  // A dummy header we write until we know better...
  //
#if CHECK_LEVEL > 0
  if (SoundStream) {
    Throw(ObjectDoesntExist,"WavSound::OpenWavFile",
	  "output sound stream exists already");
  }
#endif
  //
  // Open the audio stream for binary writing.
  SoundStream = fopen(FileName,"wb");
  if (SoundStream) {
    // File opened: Write the .wav header.
    if (wh.WriteHeader(SoundStream))
      return true;
    //
    fclose(SoundStream);    
    SoundStream = NULL;
    // Did not work. Get rid of the temporary file.
    remove(FileName);
  }
  return false;
}
///

/// WavSound::CloseWavFile
// Close the wav file, and complete the header. If the argument is
// true, then the file is assumed complete and the header is cleaned
// up. Otherwise, the file is disposed and deleted.
void WavSound::CloseWavFile(bool finewrite)
{  
  //
  // Now close the stream
  if (SoundStream) {
    // If no stream exists, do nothing about it.
    if (finewrite && Recording) {
      // Ok, try to rewind the file.
      if (fseek(SoundStream,0,SEEK_SET) == 0) {
	// Construct now the header for the written file size and
	// the sample size.
	struct WavHeader wh(OutputCounter,SamplingFreq,WavStereo,WavSixteen);
	//
	// Now write the header
	if (wh.WriteHeader(SoundStream)) {
	  if (fclose(SoundStream) == 0) {
	    // Worked fine, return result.
	    SoundStream = NULL;
	    return;
	  }
	}
      }
    }
    // The file could not setup correctly. Close the stream now.
    fclose(SoundStream);
    SoundStream = NULL;
    //
    // And remove the file
    if (FileName)
      remove(FileName);
  }
}
///

/// WavSound::UpdateSound
// Update the output sound, feed new data into the DSP.
// Delay by the timer or don't delay at all if no 
// argument given.
void WavSound::UpdateSound(class Timer *delay)
{
  // Check whether we want the sound at all. In case we don't, we
  // do not need all this.
#ifdef USE_PLAYBACK
  if (delay) {
    if (OssStream >= 0) {
      LONG fragcnt;
      struct AudioBufferBase *sn;
      // We ignore all update events since we cannot update "out of line"
      // since this would mess up the recording. But we may re-play
      // the buffers we have.
      while (delay->WaitForIO(OssStream)) {
	// Now remove buffers from the queue and replay them into the
	// device. Actually, the OSS devices should do this themselves,
	// but it seems that this doesn't work correctly!
	sn = ReadyBuffers.First();
	if (sn) {
	  // This audio node exists. Play it back node.
	  write(OssStream,sn->ReadPtr,sn->ReadyBytes());
	  // Hmm. All this could result in errors, but as the main
	  // intention is the recording of sound, I don't care.
	  // If this is not the last node, remove it, otherwise replay it.
	  // This may, of course, cause a "hickup", but we cannot
	  // regenerate audio as we need to record the audio samples
	  // at "emulation time" whereas we play them at real-time.
	  // Hence, some "lag" should be expected we need to
	  // work against. We do so by "hickup", i.e. by pushing
	  // obsolete data into the buffer again. This is not nice,
	  // but better than a "plop" and a complete buffer underrun
	  // we would have to deal otherwise. This way, the dsp queue
	  // keeps at least busy.
	  if (sn->NextOf()) {
	    sn->Remove();
	    FreeBuffers.AddTail(sn);
	  }
	}
      }
      //
      // dispose the sound nodes queued so far up to the amount we buffer.
      sn = ReadyBuffers.Last(), fragcnt = NumFrags;
      while(sn && fragcnt) {
	sn = sn->PrevOf();
	fragcnt--;
      }
      while(sn) {
	struct AudioBufferBase *prev = sn->PrevOf();
	sn->Remove();
	FreeBuffers.AddTail(sn);
	sn = prev;
      }
    } else {
      delay->WaitForEvent();
    }
  }
#else
  // Just delay up to the desired time.
  if (delay)
    delay->WaitForEvent();
#endif

}
///

/// WavSound::HBI
// Let the sound driver know that 1/15Khz seconds passed.
// This might be required for resynchronization of the
// sound driver.
// All the computations for the wav driver are done
// here since we rather need to keep the exact sampling
// frequency than to keep the timing correctly. There is
// no sound device we can synchronize to.
void WavSound::HBI(void)
{
  if (EnableSound || (Playback && OssStream >= 0)) {
    int buffersamples;
    // We may operate.
    // Update the per-frame correction to the residual which is the
    // shift in the byte output of the generated to the "ideal"
    // per-frame byte count.
    Residual     += Correction;
    buffersamples = BufferSamples;
    if (Residual >= PokeyFreq) {
      // Just overrun one sample by collecting a full frame of errors.
      buffersamples++;
      Residual  -= PokeyFreq;
    }
    // Now check whether we have to generate samples at all. If not, then there's nothing to do.
    if (PlayingBuffer && buffersamples) {
      int offset;
      // Need to generate samples from pokey. We do this manually here. The PlayingBuffer
      // is expected to be constructed here already.
#if CHECK_LEVEL > 0
      if (PlayingBuffer->Size < ULONG(buffersamples >> PlayingBuffer->SampleShift))
	Throw(OutOfRange,"WavSound::TriggerSoundScanLine","wav intermediate buffer allocated too small");
#endif
      //
      // First ask the main pokey to do the computing: Reset the buffer pointer
      // to its beginning.
      PlayingBuffer->WritePtr = PlayingBuffer->ReadPtr = PlayingBuffer->Buffer;
      LeftPokey->ComputeSamples(PlayingBuffer,buffersamples,SamplingFreq,0);
      // Check whether the buffer wants a second pokey output.
      if ((offset = PlayingBuffer->ChannelOffset())) {
	// Start at the write-offset of the second channel.
	PlayingBuffer->WritePtr  = PlayingBuffer->Buffer + offset;
	RightPokey->ComputeSamples(PlayingBuffer,buffersamples,SamplingFreq,0);
	PlayingBuffer->WritePtr -= offset;
      }
      //
      // All of this is unnecessary if we don't want to record anyhow.
      if (EnableSound) {
	// Check whether we need to check for the muting value.
	if (HaveMutingValue == false) {      
	  // Ok, check the first sample. This is the muting value. Since we must have
	  // litte endian here, we really look at the right thing even if 16bit is enabled.
	  MutingValue            = PlayingBuffer->GetSample();
	  // We could reset the read-pointer here, but what for?
	  HaveMutingValue        = true;
	}
	if (Recording == false) {
	  // Check whether we should enable recording. This happens only if
	  // there is a sample in the audio buffer that has not the muting
	  // value.
	  if (PlayingBuffer->CheckForMuting(MutingValue)) {
	    // Ok, found a value different than the muting value
	    Recording = true;
	  }
	}
      }
      //
      // Include the console speaker now. It does not trigger off muting.
      // This is intentional
      if (ConsoleSpeakerStat && EnableConsoleSpeaker) {
	PlayingBuffer->AddOffset(UBYTE(ConsoleVolume));
      }      
      //
      // We could have turned on recording right now. Hence, do so please.
      if (Recording) { 
	ULONG readybytes;
	//
	if (SoundStream == NULL) {
	  if (!OpenWavFile()) {
	    // opening or configuring the output file failed. Do not try again!
	    EnableSound = false;
	    return;
	  }
	}
	// Count the number of generated samples
	OutputCounter += buffersamples;
	//
	// Reset the buffer to its beginning again.
	PlayingBuffer->ReadPtr = PlayingBuffer->Buffer;
	// Now finally write it to the output file.
	readybytes = PlayingBuffer->ReadyBytes();
	if (fwrite(PlayingBuffer->ReadPtr,sizeof(UBYTE),readybytes,SoundStream) != (size_t)readybytes) {
	  int error = errno;
	  // Failed. Close the file immediately.
	  CloseWavFile(false);
	  // Outch. Recording failed. Should we throw an error?
	  machine->PutWarning("Generation of .wav file %s failed due to %s, "
			      "recording aborted.\n",FileName,strerror(error));
	  EnableSound = false;
	}
      }
#ifdef USE_PLAYBACK
      // Write the data to the oss device for playback.
      if (Playback && OssStream >= 0) {
	struct AudioBufferBase *abb;
	// Reset the input pointer.
	PlayingBuffer->ReadPtr = PlayingBuffer->Buffer;
	// Loop until all input samples have been converted to the
	// output.
	while(PlayingBuffer->IsEmpty() == false) {
	  int entries = FragSamples;
	  //
	  // Check how many bytes we shall allocate: At least one fragment.
	  if (entries < buffersamples)
	    entries = buffersamples;
	  //
	  // Find or create a new sound node of the required size.
	  abb = ReadyBuffers.Last();
	  if (abb == NULL || abb->FreeSamples() < ULONG(buffersamples)) {
	    // Need to find a new one.
	    abb = FreeBuffers.RemHead();
	    if (abb == NULL) {
	      // Need to get a new one.
	      abb = AudioBufferBase::NewBuffer(SignedSamples,Stereo,SixteenBit,LittleEndian,Interleaved);
	    }
	    // Add it to the output buffer to ensure that it isn't lost if we have to throw.
	    ReadyBuffers.AddTail(abb);
	    abb->Realloc(entries);
	  }
	  //
	  // Convert now from the input to the output buffer.
	  abb->CopyBuffer(PlayingBuffer);
	}
	//
	// Check whether we can feed the Oss sound driver here.
	if (Timer::CheckIO(OssStream)) {
	  abb = ReadyBuffers.First();
	  // Remove the first node, and push it into the queue.
	  if (abb) {
	    write(OssStream,abb->ReadPtr,abb->ReadyBytes());
	    // Move this buffer over to the free buffers again.
	    abb->Remove();
	    FreeBuffers.AddTail(abb);
	  }
	}
      }
#endif
    }
  }
}
///

/// WavSound::DisplayStatus
// Display the status of the sound over the monitor
void WavSound::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Audio Output Status:\n"
		   "\tAudio output enable     : %s\n"
		   "\tAudio Playback enable   : %s\n"
		   "\tAudio recording active  : %s\n"
		   "\tConsole speaker enable  : %s\n"
		   "\tConsole speaker volume  : " ATARIPP_LD "\n"
		   "\tWav output file         : %s\n"
		   "\tPlayback audio device   : %s\n"
		   "\tSampling frequency      : " ATARIPP_LD "Hz\n"
		   "\tFragment size exponent  : " ATARIPP_LD "\n"
		   "\tNumber of fragments     : " ATARIPP_LD "\n",
		   (EnableSound)?("on"):("off"),
		   (Playback)?("on"):("off"),
		   (Recording)?("on"):("off"),
		   (EnableConsoleSpeaker)?("on"):("off"),
		   ConsoleVolume,
		   FileName,
		   DspName,
		   SamplingFreq,
		   FragSize,
		   NumFrags
		   );
}
///

/// WavSound::ParseArgs
// Parse off command line arguments for the sound handler
void WavSound::ParseArgs(class ArgParser *args)
{
  bool penable = Playback;

  LeftPokey    = machine->Pokey(0);
  RightPokey   = machine->Pokey(1);
  //
  // If the output file is open, close it successfully.
  CloseWavFile(true);
  CloseOssStream();
  //
  args->DefineTitle("WavSound");
  args->DefineBool("EnableRecording","enable .wav file output",EnableSound);
  args->DefineBool("RecordAfterReset","enable recording only after a reset",EnableAfterReset);
  args->DefineBool("EnablePlayback","enable Oss audio playback",penable);
  args->DefineBool("EnableConsoleSpeaker","enable the console speaker",
		   EnableConsoleSpeaker);
  args->DefineLong("ConsoleSpeakerVolume","set volume of the console speaker",
		   0,64,ConsoleVolume);
  args->DefineFile("OutputFile","set wav output file",FileName,true,true,false);  
  args->DefineBool("ForceStereo","enforce stereo output for broken ALSA interfaces",ForceStereo);
  args->DefineBool("Stereo","generate .wav stereo output",WavStereo);
  args->DefineBool("SixteenBit","generate .wav in 16 bit resolution",WavSixteen);
  
  args->DefineString("AudioDevice","set audio output device",DspName);
  args->DefineLong("SampleFreq","set audio sampling frequency",
		   4000,48000,SamplingFreq);  
  args->DefineLong("FragSize","set the exponent of the fragment size",
		   2,16,FragSize);
  args->DefineLong("NumFrags","specify the number of fragments",
		   1,512,NumFrags);  

  // Re-read the base frequency
  PokeyFreq  = LeftPokey->BaseFrequency();
  //
#ifdef USE_PLAYBACK
  if (penable) {
    Playback = true;
    if (!OpenOssStream()) {
      // opening or configuring /dev/dsp failed. Do not try again!
      Playback = false;
    }
  } else {
    Playback = false;
  }
#endif
  //
  if (EnableAfterReset == false) {
    WarmStart();
  }
  //printf("parseargs\n");
  //WarmStart();
}
///

