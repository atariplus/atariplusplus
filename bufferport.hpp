/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: bufferport.hpp,v 1.6 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: A descendend of the render port that also allows
 ** buffering of rectangular screen regions
 **********************************************************************************/

#ifndef BUFFERPORT_HPP
#define BUFFERPORT_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "renderport.hpp"
///

/// Class BufferPort
// This structure extends the rendering mechanisms of the rastport by also offering
// the functionality of saving parts of the screen back into separate buffers.
class BufferPort : public RenderPort {
  //
public:
  // Public forward, though not for general use
  struct Backsave : public Node<struct Backsave> {
    // Contains nothing. This is intended.
  };
  //
private:
  // This buffer contains an off-screen part of the screen,
  // created by the BufferPort
  struct BacksaveNode : public Backsave {
    //
    // BufferPort this backsave buffer belongs
    // 
    class BufferPort *Port;
    //
    // Position of this off-screen buffer
    // within the port it was created with.
    LONG              LeftEdge;
    LONG              TopEdge;
    LONG              Width;
    LONG              Height;
    //
    // Buffer containing the graphics of the port
    UBYTE            *Data;
    //
    // Constructor and destructor
    BacksaveNode(class BufferPort *port,LONG le,LONG te,LONG w,LONG h);
    ~BacksaveNode(void);
  };
  //
  // A list keeping the sorted backsave buffers.
  List<struct Backsave> BacksaveList;
  //
  //
public:
  BufferPort(void);
  ~BufferPort(void);
  // 
  // Create a backsave buffer by giving its region
  // within this port. It gets rendered back by the
  // next call.
  struct Backsave *SaveRegion(LONG le,LONG te,LONG w,LONG h);
  //
  // This call restores the region and also disposes the
  // off-screen buffer.
  void RestoreRegion(struct Backsave *backsave);
};
///

///
#endif
