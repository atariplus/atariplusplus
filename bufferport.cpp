/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: bufferport.cpp,v 1.6 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: A descendend of the render port that also allows
 ** buffering of rectangular screen regions
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "bufferport.hpp"
#include "new.hpp"
#include "display.hpp"
#include "string.hpp"
///

/// BufferPort::BacksaveNode::BacksaveNode
// Create a new backsave buffer of the
// given dimensions
BufferPort::BacksaveNode::BacksaveNode(class BufferPort *port,LONG le,LONG te,LONG w,LONG h)
  : Port(port), 
    LeftEdge(le), TopEdge(te), Width(w), Height(h),
    Data(new UBYTE[w*h])
{
}
///

/// BufferPort::BacksaveNode::~BacksaveNode
// Dispose an offscreen buffer
BufferPort::BacksaveNode::~BacksaveNode(void)
{
  delete[] Data;
}
///

/// BufferPort::BufferPort
// The trivial constructor of the buffer
// port.
BufferPort::BufferPort(void)
{
}
///

/// BufferPort::~BufferPort
// The destructor deletes all
// offscreen-buffers and disposes
// their contents
BufferPort::~BufferPort(void)
{
  struct Backsave *bs;

  while((bs = BacksaveList.Last())) {
    RestoreRegion(bs);
  }
}
///

/// BufferPort::SaveRegion
// Copy a part of the renderport/bufferport
// into an off-screen buffer.
struct BufferPort::Backsave *BufferPort::SaveRegion(LONG le,LONG te,LONG w,LONG h)
{
  struct BacksaveNode *bp;
  UBYTE *src,*dst;

  bp = new struct BacksaveNode(this,le,te,w,h);
  BacksaveList.AddHead(bp);
  //
  // Now copy the data over row by row.
  src = At(le,te);
  dst = bp->Data;
  do {
    memcpy(dst,src,w);
    dst += w;
    src += Modulo;
  } while(--h);

  return bp;
}
///

/// BufferPort::RestoreRegion
// Restore an off-screen region by coping it
// back and then disposing it (afterwards)
void BufferPort::RestoreRegion(struct Backsave *b)
{
  struct BacksaveNode *bs = (struct BacksaveNode *)b;
  UBYTE *src,*dst;
  LONG w,h,c;

  src = bs->Data;
  dst = At(bs->LeftEdge,bs->TopEdge);
  w   = bs->Width;
  h   = bs->Height;  
  c   = h;

  do {
    memcpy(dst,src,w);
    dst += Modulo;
    src += w;
  } while(--c);
  // Signal that this rectangle requires refresh
  Screen->SignalRect(bs->LeftEdge+Xo,bs->TopEdge+Yo,w,h);
  // Remove from the list and let it go.
  bs->Remove();
  delete bs;
}
///
