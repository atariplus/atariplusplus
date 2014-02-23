/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_truecolorbuffer.cpp,v 1.17 2014/01/08 20:07:10 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

/// Includes
#include "types.h"
#include "monitor.hpp"
#include "argparser.hpp"
#include "x11_truecolorbuffer.hpp"
#include "xfront.hpp"
#include "gtia.hpp"
#include "antic.hpp"
#include "screendump.hpp"
#include "exceptions.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "new.hpp"
#ifndef X_DISPLAY_MISSING
///

/// Statics
#ifndef HAS_MEMBER_INIT
const int X11_TrueColorBuffer::RenderBufferSize = 16;
const int X11_TrueColorBuffer::ScanBuffNum      = 64;
#endif
///

/// X11_TrueColorBuffer::X11_TrueColorBuffer
X11_TrueColorBuffer::X11_TrueColorBuffer(class Machine *mach, class XFront *front)
  : X11_DisplayBuffer(mach,front),
    active(NULL), last(NULL), row(NULL), changeflags(NULL),
    idxactive(NULL), idxrow(NULL),
    enforcefullrefresh(true), indexdirty(false), nextscanblock(16),
    ScanBuffer(new struct ScanBlock*[ScanBuffNum])
{
  memset(ScanBuffer,0,sizeof(ScanBuffer) * ScanBuffNum);
}
///

/// X11_TrueColorBuffer::~X11_TrueColorBuffer
X11_TrueColorBuffer::~X11_TrueColorBuffer(void)
{
  // Unlink us from the X system here.
  DetachFromX();  
  delete[] ScanBuffer;
  // Delete the scanbuffers
  delete[] active;
  delete[] last;
  delete[] changeflags;
  delete[] idxactive;
}
///

/// X11_TrueColorBuffer::DetachFromX
// Unlink the display buffer from the X system
void X11_TrueColorBuffer::DetachFromX(void)
{
  int i;
  //
  // Release all ScanBuffers
  for(i = 0;i<ScanBuffNum;i++) {
    struct ScanBlock *buf = ScanBuffer[i];
    if (buf) {
      ReleaseScanBlock(buf);
      delete buf;
    }
    ScanBuffer[i] = NULL;
  }
  //
  //  
  // Perform this on the super class
  CloseX();
}
///

/// X11_TrueColorBuffer::ScanBlock::AddPixel
// Add a pixel to the ScanBuffer, possibly draw the contents if
// we are too full. x and y are the coordinates (in the atari screen)
// w is the number of pixels, h is the height in atari lines.
void X11_TrueColorBuffer::ScanBlock::AddPixel(int x,int y,int w, int h)
{
  XRectangle *current;
  // compute the start coordinates of this pixel
  x *= width;
  y *= height;
  w *= width;
  h *= height;
  //
  // Check whether we have any entries. If so, it might be that
  // it is enough to enlarge the previous rectangle
  if (entries) {
    int test = entries - 1;
    XRectangle *last = rectangles+test;
    // Check whether this connects fine to the last rectangle.
    if (last->x + last->width == x && last->y == y && last->height == h) {
      // Yup, connects fine to it. Just enlarge me horizontally
      last->width += w;
      return;
    } 
    //
    // Now test whether we have pixels one row above
    while(test >= 0) {
      int lastend = last->y + last->height;
      if (lastend == y && last->x == x && last->width == w) {
	// Yup, connects fine to it. Enlarge me vertically
	last->height += h;
	return;
      }
      last--;
      test--;
    }
  }
  //
  // Otherwise check whether we have still enough room in here. If not,
  // then drop out.
  if (entries >= RenderBufferSize) {
    FlushBlock(); // Flush me out
  }
  current         = rectangles+entries;
  current->x      = WORD(x);
  current->y      = WORD(y);
  current->width  = WORD(w);
  current->height = WORD(h);
  entries++;
}
///

/// X11_TrueColorBuffer::ScanBlock::FlushBlock
// Write out one block of the scan buffer to the display
// Flush the block and allow reusal: We need to specify:
// A target display to draw on, a drawable we fill into and
// an array of 256 graphic contexts that are setup for the
// foreground/background combinations.
void X11_TrueColorBuffer::ScanBlock::FlushBlock(void)
{
  XFillRectangles(display,target,context,rectangles,entries);
  entries = 0;
}
///

/// X11_TrueColorBuffer::RebuildScreen
// Rebuild the screen. If differential is set, then only an incremental
// update to the last screen is made. Otherwise, a full screen
// rebuild is made.
void X11_TrueColorBuffer::RebuildScreen(bool differential)
{
  int xstart,ystart,x,y,w,h;
  PackedRGB *row,*entry;
  PackedRGB *lastrow,*lastentry;
  bool   update = false;

  if (active == NULL || display == 0 || mapped == false)
    return;

  if (indexdirty) {
    PackedRGB *row;
    UBYTE     *idxrow;
    const struct ColorEntry *colormap = machine->GTIA()->ActiveColorMap();
    LONG       height = Height;
    //
    // Compute the start addresses of the row/packed row.
    row    = active;
    idxrow = idxactive;
    do {
      LONG i = modulo;
      do {
	*row  = colormap[*idxrow].XPackColor();
	row++,idxrow++;
      } while(--i);
    } while(--height);
    indexdirty = false;
  }

  if (differential == false) {
    update       = true;
  }
  if (enforcefullrefresh || last == NULL) {
    update       = true;
    differential = false;
  }
  //
  //
  // Get the pointer to the first row we want to render on the screen.
  row     = active + LeftEdge + TopEdge * modulo;
  lastrow = last   + LeftEdge + TopEdge * modulo;
  for(y = 0;y<Height;y++,row += modulo,lastrow += modulo) {
    // Make this a height one row, remember y start position
    h         = 1;
    ystart    = y;
    // Check whether this is an incremental update and this row looks
    // like the one in the last frame. If so, then skip this row completely.
    if (differential) {
      if (changeflags[y+1] == 0)
	continue;
    }
    //
    // Ok, this row requires refresh. Check whether the row below should
    // it exist looks like this row. If so, increment the height of this
    // row and advance y.
    while(y < Height-1) {
      if (memcmp(row,row+modulo,Width * sizeof(PackedRGB)) == 0) {
	// If we are differential, we must also compare the last rows
	// in the same way as we might otherwise overlook an update in
	// one of the skipped rows.
	if (differential && memcmp(lastrow,lastrow+modulo,Width * sizeof(PackedRGB)))
	  break;
	// Yup, it does. Next y, increment height.
	row     += modulo;
	lastrow += modulo;
	h++;
	y++;
      } else break;
    }
    //
    // Now get pointer to the start of the line
    entry       = row;
    lastentry   = lastrow;
    //
    // Now go for all points in this scanline
    for(x=0;x<Width;x++,entry++,lastentry++) {
      struct ScanBlock *sb;
      // Make this a width 1 pixel, remember start position.
      w      = 1;
      xstart = x;
      //
      // If we are differential and pixels don't differ, skip them.
      if (differential && *entry == *lastentry)
	continue;
      //
      // Check whether the next pixel has still the same color. If so,
      // increment the width and combine pixels.
      while (x < Width-1) {
	if (entry[0] == entry[1]) {
	  x++;
	  w++;
	  entry++;
	  lastentry++;
	} else break;
      }
      // Get the scanblock that is responsible for this color
      //
      sb = FindBlock(*entry);
      if (sb) {
	// and add the pixel.
	sb->AddPixel(xstart,ystart,w,h);
	update = true;
      }
    }
  }
  //
  // Now flush all pixel buffers out.
  for(x = 0;x < ScanBuffNum; x++) {
    if (ScanBuffer[x] && ScanBuffer[x]->entries) {
      ScanBuffer[x]->FlushBlock();
    }
  }
  //
  // If we require an update and have an intermediate pixmap, render it now
  // onto the screen.
  if (update && PixMapIndirect) {
    XCopyArea(display,pixmap,window,pixmapcontext,
	      0,0,PixelWidth*Width,PixelHeight*Height,
	      0,0);
  }
  //
  // As we re-drew the window now, we no longer need a full refresh next time
  if (last) 
    enforcefullrefresh = false;
  if (changeflags)
    memset(changeflags,0,bufferlines+2);
}
///

/// X11_TrueColorBuffer::NextBuffer
// Return the next active buffer and swap buffers
UBYTE *X11_TrueColorBuffer::NextBuffer(void)
{
  PackedRGB *tmp;

  tmp    = active;
  active = last;
  last   = tmp;
  //
  // Check whether we have an active buffer. If not, we have to build it now.
  if (active == NULL || changeflags == NULL || idxactive == NULL) {
    UWORD w,h;
    //
    // Get the dimensions required for the buffer from antic
    machine->Antic()->DisplayDimensions(w,h);
    //
    if (active == NULL) {
      // and build it.
      active = new PackedRGB[w*h];
    }
    if (idxactive == NULL) {
      idxactive = new UBYTE[w*h];
    }
    if (changeflags == NULL) {
      changeflags = new UBYTE[h + 2];
      memset(changeflags,1,h+2);
    }
    //
    // Keep the modulo to ensure the refresh is right.
    modulo      = w;
    bufferlines = h;
  }
  
  // Reset the scan counter as well.
  row     = active;
  idxrow  = idxactive;
  lastrow = last;
  curflag = changeflags + 1;
  return idxactive;
}
///

/// X11_TrueColorBuffer::DumpScreen
// Make a screen dump as .ppm file
void X11_TrueColorBuffer::DumpScreen(FILE *file,ScreenDump::GfxFormat format)
{  
  class ScreenDump dumper(machine,machine->GTIA()->ActiveColorMap(),
			  LeftEdge,TopEdge,Width,Height,modulo,format);
  
  // Just use the dumper to get the jump done.
  if (active) {
    dumper.Dump(active,file);
  }
}
///

/// X11_TrueColorBuffer::HandleExposure
// In case of a window exposure event, try to rebuild the window
// contents. This works only if we use an indirect drawing method
// using a pixmap. Otherwise, we ignore the event as we refresh
// the screen next time anyhow.
void X11_TrueColorBuffer::HandleExposure(void)
{
  enforcefullrefresh = true;  // ensure that the next redraw is a complete redraw
  mapped             = true;  // the window is now available. (initial exposure)
  if (PixMapIndirect && display) {
    XCopyArea(display,pixmap,window,pixmapcontext,
	      0,0,PixelWidth*Width,PixelHeight*Height,
	      0,0);
  }
}
///

/// X11_TrueColorBuffer::ReleaseScanBlock
// Release the resources for the scan block 
// without flushing it and without releasing the
// administration structure.
void X11_TrueColorBuffer::ReleaseScanBlock(struct ScanBlock *buf)
{
  if (buf->alloc) {
    // Release the color for this scan block.
    XFreeGC(display,buf->context);
    XFreeColors(display,cmap,&buf->xpen,1,0);
    buf->alloc = false;
  }
}
///

/// X11_TrueColorBuffer::FindBlock
// Find the scanbuffer to a given color or create it if it doesn't
// exist.
struct X11_TrueColorBuffer::ScanBlock *X11_TrueColorBuffer::FindBlock(PackedRGB color)
{
  int i;
  struct ScanBlock *buf,**p;
  // First pack the color so we can search more efficiently.
  //
  // Now check the scan buffer array whether we have this color already.
  for(i = 0, p = ScanBuffer; i < ScanBuffNum; i++,p++) {
    if (*p) {
      if ((*p)->alloc && (*p)->color == color) {
	// Implement a move-to-front logic to keep the search time low.
	if (i > 16) {
	  buf = *p;
	  memmove(ScanBuffer + 1, ScanBuffer, i * sizeof(struct ScanBlock *));
	  ScanBuffer[0] = buf;
	  return buf;
	}
	return *p;
      }
    }
  }
  //
  //
  // Now try to find a new slot for the color. This means that
  // we might have to release some colors we have...
  for(i = 0;i < ScanBuffNum; i++) {
    // Ok, found no fitting scanblock. Check whether the current one
    // is free. If so, flush it and release it.
    if ((buf = ScanBuffer[nextscanblock])) {
      if (buf->alloc) {
	buf->FlushBlock();
	// Release the X resources for this block.
	ReleaseScanBlock(buf);
      }
    } else {
      // Otherwise, allocate a new one.
      ScanBuffer[nextscanblock] = new struct ScanBlock(display,(PixMapIndirect)?(pixmap):(window),
						       PixelWidth,PixelHeight);
    }
    //
    // Now allocate the X resources for this block.
    buf = ScanBuffer[nextscanblock];
    {
      unsigned long pen;
      //
      // Advance the allocation pointer to the next position.
      nextscanblock++;
      if (nextscanblock >= ScanBuffNum) {
	nextscanblock = 0; 
      }
      //
      //
      if (XAllocPen(pen,color)) {
	XGCValues xgv;
	GC newgc;
	// Create a graphic context for this scan buffer.
	xgv.foreground     = pen;
	xgv.background     = pen;
	newgc              = XCreateGC(display,window,GCForeground | GCBackground,&xgv);
	if (newgc) {
	  // Ok, we got all we need. Now insert the data into the
	  // new scan buffer.
	  buf->xpen        = pen;
	  buf->context     = newgc;
	  buf->color       = color;
	  buf->alloc       = true;
	  //
	  return buf;
	}
	//
	// Outch. Did not work. Release the pen.
	XFreePen(pen);
      }
    }
    // And try again to find a free slot.
  }
  return NULL;
}
///

/// X11_TrueColorBuffer::XAllocPen
// Allocate an RGB pen from the X system. Returns true in case we
// got it.
bool X11_TrueColorBuffer::XAllocPen(unsigned long &pen,PackedRGB packed)
{
  XColor color;

  color.pixel   = 0;
  color.red     = UWORD(((packed >> 16) & 0xff) * 0x0101);
  color.green   = UWORD(((packed >> 8 ) & 0xff) * 0x0101);
  color.blue    = UWORD(((packed >> 0 ) & 0xff) * 0x0101);
  color.flags   = DoRed|DoGreen|DoBlue;
  color.pad     = 0;
  
  if (XAllocColor(display,cmap,&color)) {
    pen = color.pixel;
    return true;
  }
  return false;
}
///

/// X11_TrueColorBuffer::XFreePen
// Release a pen for the X11 display system.
void X11_TrueColorBuffer::XFreePen(unsigned long pen)
{
  XFreeColors(display,cmap,&pen,1,0);
}
///

/// X11_TrueColorBuffer::ConnectToX
// Connect to the X11 window system
void X11_TrueColorBuffer::ConnectToX(Display *d,Screen *s,Window win,Colormap cm,
				     LONG le,LONG te,LONG w,LONG h,
				     LONG pxwidth,LONG pxheight,bool indirect)
{  
  int i;
  //
  enforcefullrefresh = true;  // ensure that the next redraw is a complete redraw
  //
#if CHECK_LEVEL > 2
  if (pixmap || pixmapcontext) {
    Throw(ObjectExists,"X11_TrueColorBuffer::ConnectToX",
	  "The display buffer is already connected to the X system");
  }
#endif
  // Get window, colormap, screen, display: All this in the
  // super class.
  SetupX(d,s,win,cm,le,te,w,h,pxwidth,pxheight,indirect);
  //
  if (pixmap) {
    unsigned long pen;
    XGCValues xgv;
    // Define the foreground and background color: Both black
    //
    if (XAllocPen(pen,colormap[0].XPackColor())) {
      xgv.foreground = pen;
      xgv.background = pen;
      //
      pixmapcontext  = XCreateGC(display,window,GCForeground | GCBackground,&xgv);
      XFreePen(pen);
    } else {
      pixmapcontext  = 0;
    }
    if (pixmapcontext == 0) {
      XFreePixmap(display,pixmap);
      PixMapIndirect = false;
    } else {
      // Clear the pixmap to avoid display artefacts
      XFillRectangle(display,pixmap,pixmapcontext,
		     0,0,Width*PixelWidth,Height*PixelHeight);
    }
  }
  //
  // Release all scanbuffer X resources.
  for(i = 0;i<ScanBuffNum;i++) {
    struct ScanBlock *buf = ScanBuffer[i];
    if (buf) {
      ReleaseScanBlock(buf);
      delete buf;
    }
    ScanBuffer[i] = NULL;
  }
}
///

/// X11_TrueColorBuffer::ActiveBuffer
// Return the currently active screen buffer
UBYTE *X11_TrueColorBuffer::ActiveBuffer(void)
{
  // In case we do not yet have a buffer, generate one
  if (idxactive == NULL || active == NULL) {
    return NextBuffer();
  }
  return idxactive;
}
///

/// X11_TrueColorBuffer::ColdStart
// Run a power-up of the display buffer here.
void X11_TrueColorBuffer::ColdStart(void)
{  
  // Delete the screen buffers
  delete[] active;
  delete[] last;
  delete[] changeflags;
  active      = NULL;
  last        = NULL;
  changeflags = NULL;
  // This also implies a warmstart
  WarmStart();
}
///

/// X11_TrueColorBuffer::WarmStart
// Run a regular reset of the display buffer
void X11_TrueColorBuffer::WarmStart(void)
{
  // Enforce a full window refresh for the next go
  enforcefullrefresh = true;  // ensure that the next redraw is a complete redraw
}
///

/// X11_TrueColorBuffer::DisplayStatus
void X11_TrueColorBuffer::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("X11_TrueColorBuffer status:\n"
		   "\tIndirect rendering    : %s\n"
		   "\tPixel width           : " LD "\n"
		   "\tPixel height          : " LD "\n"
		   "\tTrue Color Renderer   : on\n"
		   "\tLeftEdge              : " LD "\n"
		   "\tTopEdge               : " LD "\n"
		   "\tWidth                 : " LD "\n"
		   "\tHeight                : " LD "\n",
		   (PixMapIndirect)?("on"):("off"),
		   PixelWidth,PixelHeight,
		   LeftEdge,TopEdge,Width,Height);
}
///

/// X11_TrueColorBuffer::PushLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void X11_TrueColorBuffer::PushLine(UBYTE *in,int size)
{
  PackedRGB *outbuf = row;
  PackedRGB *out    = outbuf;
  int i             = size;  
  const struct ColorEntry *colormap = machine->GTIA()->ActiveColorMap();

  if (outbuf) {
    // Convert this line to RGB, then push into the
    // true color buffer.
    do {
      *out = colormap[*in].XPackColor();
      out++,in++;
    } while(--i);
    
    PushRGBLine(outbuf,i);
  }
}
///

/// X11_TrueColorBuffer::PushRGBLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void X11_TrueColorBuffer::PushRGBLine(PackedRGB *,int)
{
  if (lastrow) {   
    // Check whether anything changed. If so,
    // set the changeflag.
    if (memcmp(lastrow,row,modulo * sizeof(PackedRGB))) {
      curflag[0] = curflag[1] = 1;
    }
    lastrow += modulo;
    curflag++;
  }
  row     += modulo;
  idxrow  += modulo;
}
///

/// X11_TrueColorBuffer::ResetVertical
// Signal that we start the display generation again from top. Hence, this implements
// some kind of "vertical sync" signal for the display generation.
void X11_TrueColorBuffer::ResetVertical(void)
{
  idxrow   = ActiveBuffer();
  row      = active;
  lastrow  = last;
  curflag  = changeflags + 1;
}
///

/// X11_TrueColorBuffer::SignalRect
// Signal the requirement for refresh in the indicated region. This is only
// used by the GUI engine as the emulator core uses PushLine for that.
// Not required here... X11 will find out itself.
void X11_TrueColorBuffer::SignalRect(LONG,LONG,LONG,LONG)
{
  indexdirty = true;
}
///

///
#endif // of if HAVE_LIBX11
///
