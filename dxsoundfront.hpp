/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: dxsoundfront.hpp,v 1.4 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: wrapper class for the DirectX sound interface
 **********************************************************************************/

#ifndef DXSOUNDFRONT_HPP
#define DXSOUNDFRONT_HPP

/// Includes
#include "types.h"
///

/// Forwards
// The DXWrapper hides all M$ specific pointers away and keeps
// the real interface.
struct DXWrapper;
///

/// Class DXSoundFront
// This is the wrapper class for direct sound interfacing
class DXSound {
  //
  //
  // Pointer to the real thing. We are here actually
  // following the pImpl-Idiom and defer the tough job to
  // this sub-class.
  struct DXWrapper *wrap;
  //
  //
public:
  DXSound(void);
  ~DXSound(void);
  //
  // Setup for the given characteristics: Stereo (or not). This
  // setting might not be honored if stereo is not available.
  // Frequency (might not be honored) sample depth (8 or 16 bit)
  // This requires a "handle" to a Window.
  // The chunkexp describes the length of one buffer as the exponent
  // to the power of two.
  // The buffer settings the number of buffers to be allocated for it.
  // Returns a true/false indicator.
  bool SetupDXSound(void *window,int nChannels,int frequency,
		    int depth,int chunkexp,int nbuffers);
  //
  // Read characteristics of the truely selected interface. Might be different from
  // what was required.
  //
  // Return the number of available channels. Might be one or two.
  int ChannelsOf(void);
  //
  // Return the sampling frequency in Hz
  int FrequencyOf(void);
  //
  // Return the size of one buffer in bytes
  int ChunkSizeOf(void);
  //
  // Return the number of buffers.
  int NumBuffersOf(void);
  //
  // Return the precision of the channels.
  int ChannelDepthOf(void);
  //
  // Shutdown the sound. Also called on destruction.
  void CloseSound(void);
  //
  //
  // Return the next available buffer for fill-in, or NULL in case
  // we are currently playing and enough buffers are filled. Must
  // release this buffer again when done with the fill-in.
  // If wait is non-zero, this call locks until a buffer is available
  // if we are playing or at most the given number of milliseconds.
  void *NextBuffer(int &size,int wait);
  //
  // After having filled in data, release it with the following
  // call.
  void ReleaseBuffer(void *buffer,int size);
  //
  // Start the sound output now
  bool Start(void);
  //
  // Stop sound output.
  void Stop(void);
  //
  // Return an indicator whether we are currently playing.
  bool IsActive(void);
  //
  //
  // return a handle to the current SDL window in case we have it
  static void *GetSDLWindowHandle(void);
};
///

///
#endif
