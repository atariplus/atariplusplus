/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_mappedbuffer.cpp,v 1.11 2014/06/01 20:07:53 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

/// Includes
#include "types.h"
#include "monitor.hpp"
#include "argparser.hpp"
#include "x11_mappedbuffer.hpp"
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
const int X11_MappedBuffer::RenderBufferSize = 16;
const int X11_MappedBuffer::ScanBuffNum      = 256;
#endif
///

/// X11_MappedBuffer::X11_MappedBuffer
X11_MappedBuffer::X11_MappedBuffer(class Machine *mach, class XFront *front)
  : X11_DisplayBuffer(mach,front),
    active(NULL), last(NULL), row(NULL),
    graphiccontexts(NULL), pens(NULL),
    enforcefullrefresh(true),
    ScanBuffer(new struct ScanBlock*[ScanBuffNum])
{
  memset(ScanBuffer,0,sizeof(ScanBuffer) * ScanBuffNum);
}
///

/// X11_MappedBuffer::~X11_MappedBuffer
X11_MappedBuffer::~X11_MappedBuffer(void)
{
  // Unlink us from the X system here.
  DetachFromX();  
  delete[] ScanBuffer;
  ScanBuffer = NULL;  
  // Delete the scanbuffers
  delete[] active;
  active = NULL;
  delete[] last;
  last   = NULL;
}
///

/// X11_MappedBuffer::DetachFromX
// Unlink the display buffer from the X system
void X11_MappedBuffer::DetachFromX(void)
{
  int i;
  //
  // Release all ScanBuffers
  for(i = 0;i<ScanBuffNum;i++) {
    delete ScanBuffer[i];
    ScanBuffer[i] = NULL;
  }
  //
  if (graphiccontexts) {
    int i;
    for(i=0;i<256;i++) {
      if (graphiccontexts[i])
	XFreeGC(display,graphiccontexts[i]);
    }
    delete[] graphiccontexts;
    graphiccontexts = NULL;
  }
  //
  // Dispose the pens should we have some
  if (pens) {
    int i;
    for(i=0;i<256;i++) {
      if (pens[i].alloc) {
	XFreeColors(display,cmap,&pens[i].pen,1,0);
      }
    }
    delete[] pens;
    pens = NULL;
  }  
  //  
  // Perform this on the super class
  CloseX();
  //
}
///

/// X11_MappedBuffer::ScanBlock::AddPixel
// Add a pixel to the ScanBuffer, possibly draw the contents if
// we are too full. x and y are the coordinates (in the atari screen)
// w is the number of pixels, h is the height in atari lines.
void X11_MappedBuffer::ScanBlock::AddPixel(int x,int y,int w, int h)
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
  if (entries > 0) {
    int test = entries - 1;
    XRectangle *last = rectangles + test;
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

/// X11_MappedBuffer::ScanBlock::FlushBlock
// Write out one block of the scan buffer to the display
// Flush the block and allow reusal: We need to specify:
// A target display to draw on, a drawable we fill into and
// an array of 256 graphic contexts that are setup for the
// foreground/background combinations.
void X11_MappedBuffer::ScanBlock::FlushBlock(void)
{
  XFillRectangles(display,target,context,rectangles,entries);
  entries = 0;
}
///

/// X11_MappedBuffer::RebuildScreen
// Rebuild the screen. If differential is set, then only an incremental
// update to the last screen is made. Otherwise, a full screen
// rebuild is made.
void X11_MappedBuffer::RebuildScreen(bool differential)
{
  int xstart,ystart,x,y,w,h;
  UBYTE *row,*entry;
  UBYTE *lastrow,*lastentry;
  bool   update = false;

  if (active == NULL  || display == 0 || 
      mapped == false || graphiccontexts == NULL)
    return;

  colormap      = machine->GTIA()->ActiveColorMap();

  if (differential == false) {
    update       = true;
  }
  if (enforcefullrefresh || last == NULL) {
    update       = true;
    differential = false;
  }
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
      if (memcmp(row,lastrow,Width) == 0)
	continue;
    }
    //
    // Ok, this row requires refresh. Check whether the row below should
    // it exist looks like this row. If so, increment the height of this
    // row and advance y.
    while(y < Height-1) {
      if (memcmp(row,row+modulo,Width) == 0) {
	// If we are differential, we must also compare the last rows
	// in the same way as we might otherwise overlook an update in
	// one of the skipped rows.
	if (differential && memcmp(lastrow,lastrow+modulo,Width))
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
    entry     = row;
    lastentry = lastrow;
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
      sb = FindBlock(*entry);
      //
      // and add the pixel.
      sb->AddPixel(xstart,ystart,w,h);
      update = true;
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
  if (last) enforcefullrefresh = false;
}
///

/// X11_MappedBuffer::NextBuffer
// Return the next active buffer and swap buffers
UBYTE *X11_MappedBuffer::NextBuffer(void)
{
  UBYTE *tmp;

  tmp    = active;
  active = last;
  last   = tmp;
  //
  // Check whether we have an active buffer. If not, we have to build it now.
  if (active == NULL) {
    UWORD w,h;
    //
    // Get the dimensions required for the buffer from antic
    machine->Antic()->DisplayDimensions(w,h);
    //
    // and build it.
    active = new UBYTE[w*h];
    //
    // Keep the modulo to ensure the refresh is right.
    modulo = w;
  }
  
  // Reset the scan counter as well.
  row = active;
  return active;
}
///

/// X11_MappedBuffer::DumpScreen
// Make a screen dump as .ppm file
void X11_MappedBuffer::DumpScreen(FILE *file,ScreenDump::GfxFormat format)
{
  class ScreenDump dumper(machine,machine->GTIA()->ActiveColorMap(),
			  LeftEdge,TopEdge,Width,Height,modulo,format);

  // Just use the dumper to get the jump done.
  if (active) {
    dumper.Dump(active,file);
  }
}
///

/// X11_MappedBuffer::HandleExposure
// In case of a window exposure event, try to rebuild the window
// contents. This works only if we use an indirect drawing method
// using a pixmap. Otherwise, we ignore the event as we refresh
// the screen next time anyhow.
void X11_MappedBuffer::HandleExposure(void)
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

/// X11_MappedBuffer::AllocateColors
// Request all colors from the colormap
int X11_MappedBuffer::AllocateColors(void)
{
  int i,missing = 0; // # of missing colors
  //
  // Build the array containing the allocated pens.
  pens = new Pen[256];
  // Allocate colors. First go: Allocate 128 colors to fill
  // every second color. Actually, that's only 120 since hue 1
  // equals hue 15.
  // I need at least that much!
  for (i=0;i<256;i+=2) {
    XColor color;
    
    color.red     = UWORD(colormap[i].red   * 0x0101);
    color.green   = UWORD(colormap[i].green * 0x0101);
    color.blue    = UWORD(colormap[i].blue  * 0x0101);
    color.flags   = DoRed|DoGreen|DoBlue;

    if (XAllocColor(display,cmap,&color)) {
      pens[i].pen   = color.pixel;
      pens[i+1].pen = color.pixel;
      pens[i].alloc = true;
    } else {
      Throw(ObjectDoesntExist,"X11_MappedBuffer::AllocateColors",
	    "Atari++ requires at least 120 free colors.\n"
	    "Try \"-PrivateCMap on\" on the command line.");
    }
  }
  //
  // 2nd go: Allocate the remaining colors. Start with the hue 0 
  for(i=1;i<256;i+=2) {
    XColor color;
    
    color.red     = UWORD(colormap[i].red   * 0x0101);
    color.green   = UWORD(colormap[i].green * 0x0101);
    color.blue    = UWORD(colormap[i].blue  * 0x0101);
    color.flags   = DoRed|DoGreen|DoBlue;

    if (XAllocColor(display,cmap,&color)) {
      pens[i].pen   = color.pixel;
      pens[i].alloc = true;
    } else {
      missing++;
    }
  }

  return missing;
}
///

/// X11_MappedBuffer::ConnectToX
// Connect to the X11 window system
void X11_MappedBuffer::ConnectToX(Display *d,Screen *s,Window win,Colormap cm,
				  LONG le,LONG te,LONG w,LONG h,
				  LONG pxwidth,LONG pxheight,bool indirect)
{  
  int missing = 0;
  //
  enforcefullrefresh = true;  // ensure that the next redraw is a complete redraw
  //
#if CHECK_LEVEL > 2
  if (pens || pixmap || pixmapcontext || graphiccontexts) {
    Throw(ObjectExists,"X11_MappedBuffer::ConnectToX",
	  "The display buffer is already connected to the X system");
  }
#endif
  // Get window, colormap, screen, display: All this in the
  // super class.
  SetupX(d,s,win,cm,le,te,w,h,pxwidth,pxheight,indirect);
  //
  // Allocate all the pens (pixel colors) 
  // Check whether we need to allocate the pens here
  missing = AllocateColors();
  //  
  if (pixmap) {
    XGCValues xgv;
    // Define the foreground and background color: Both black
    //
    memset(&xgv,0,sizeof(xgv));
    xgv.foreground = pens[0].pen;
    xgv.background = pens[0].pen;
    //
    pixmapcontext  = XCreateGC(display,window,GCForeground | GCBackground,&xgv);
    if (pixmapcontext == 0) {
      XFreePixmap(display,pixmap);
      PixMapIndirect = false;
    } else {
      // Clear the pixmap to avoid display artefacts
      XFillRectangle(display,pixmap,pixmapcontext,
		     0,0,Width*PixelWidth,Height*PixelHeight);
    }
  }
  // Now build all the graphic contexts for all the rectangles we use
  // for rendering.
  {
    XGCValues xgv;
    int i;
    graphiccontexts = new GC[256];
    // Let's hope zero means no GC. I just don't know, but have no further
    // information about it. The man pages say nothing at all.
    memset(graphiccontexts,0,sizeof(GC) * 256);
    //
    // Now try to allocate it.
    for(i=0;i<256;i++) {
      GC newgc;
      xgv.foreground     = pens[i].pen;
      xgv.background     = pens[0].pen;
      newgc              = XCreateGC(display,window,GCForeground | GCBackground,&xgv);
      if (newgc == 0)
	break;
      graphiccontexts[i] = newgc;
    }
    // Check whether the creation worked. If it doesn't, throw.
    // The destructor will cleanup the partially allocated
    // graphic contexts.
    if (i<256)
      Throw(ObjectDoesntExist,"X11_MappedBuffer::ColdStart",
	    "Failed to allocate the graphic contexts");
  }
  //
  // Finally, deliver an error if we couldn't get all colors.
  if (missing) {
    machine->PutWarning("Failed to allocate %d colors of 256, winging it.\n"
			"Try to enable \"PrivateCMap\" flag in the X11 menu.\n",missing);
  }
}
///

/// X11_MappedBuffer::ActiveBuffer
// Return the currently active screen buffer
UBYTE *X11_MappedBuffer::ActiveBuffer(void)
{
  // In case we do not yet have a buffer, generate one
  if (active == NULL) {
    return NextBuffer();
  }
  return active;
}
///

/// X11_MappedBuffer::ColdStart
// Run a power-up of the display buffer here.
void X11_MappedBuffer::ColdStart(void)
{  
  // Delete the scanbuffers
  delete[] active;
  delete[] last;
  active = NULL;
  last   = NULL;
  // This also implies a warmstart
  WarmStart();
}
///

/// X11_MappedBuffer::WarmStart
// Run a regular reset of the display buffer
void X11_MappedBuffer::WarmStart(void)
{
  // Enforce a full window refresh for the next go
  enforcefullrefresh = true;  // ensure that the next redraw is a complete redraw
}
///

/// X11_MappedBuffer::DisplayStatus
void X11_MappedBuffer::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("X11_MappedBuffer status:\n"
		   "\tIndirect rendering    : %s\n"
		   "\tPixel width           : " LD "\n"
		   "\tPixel height          : " LD "\n"
		   "\tTrue Color Renderer   : off\n"
		   "\tLeftEdge              : " LD "\n"
		   "\tTopEdge               : " LD "\n"
		   "\tWidth                 : " LD "\n"
		   "\tHeight                : " LD "\n",
		   (PixMapIndirect)?("on"):("off"),
		   PixelWidth,PixelHeight,
		   LeftEdge,TopEdge,Width,Height);
}
///

///
#endif // of if HAVE_LIBX11
///
