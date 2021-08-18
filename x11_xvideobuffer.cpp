/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_xvideobuffer.cpp,v 1.4 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

/// Includes
#include "types.h"
#include "monitor.hpp"
#include "argparser.hpp"
#include "x11_xvideobuffer.hpp"
#include "xfront.hpp"
#include "gtia.hpp"
#include "antic.hpp"
#include "screendump.hpp"
#include "exceptions.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "new.hpp"
#ifndef X_DISPLAY_MISSING
#ifdef X_USE_XV
#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>  // must include XShmQueryExtension
#include <X11/extensions/Xvlib.h> // must include XvQueryExtension
///

/// XVideoShmMem
// The shared video extension house keeping structure.
struct XVideoShMem {
  //
  // The X11 display on which this all happens.
  Display        *display;
  //
  // The X11 window.
  Window          window;
  //
  // The Xv port to renter through.
  XvPortID        port;
  //
  // The video format, as GUID.
  int             format;
  //
  // Shared memory segment info. 
  XShmSegmentInfo shminfo;
  //
  // The Xv Image
  XvImage        *image;
  //
  // The shared memory segment.
  int             memhandle;
  //
  // The mapped shared memory.
  void           *shmem;
  //
  // The graphics context
  GC              context;
  //
  // Dimensions.
  LONG            width;
  LONG            height;
  //
public:
  XVideoShMem(Display *d,Window win,XvPortID vport,int vformat,LONG w,LONG h)
    : display(d), window(win), port(-1), format(vformat), image(NULL), memhandle(-1), shmem(NULL),
      context(NULL), width(w), height(h)
  {
    if (XvGrabPort(d,vport,CurrentTime) == Success) {
      int i,count;
      port  = vport;
      //
      // Set autopaint on in case it has been disabled.
      static const char autopaint[] = "XV_AUTOPAINT_COLORKEY";
      const XvAttribute * const attr = XvQueryPortAttributes(d, port, &count);
      for (i = 0; i < count; ++i) {
	if (!strcmp(attr[i].name, autopaint)) {
	  const Atom atom = XInternAtom(d, autopaint, False);
	  if (atom != None) {
	    XvSetPortAttribute(d, port, atom, 1);
	    break;
	  }
	}
      }
      //
      image = XvShmCreateImage(d,port,format,NULL,w,h,&shminfo);
      if (image) {
	// Got the image.
	memhandle = shmget(IPC_PRIVATE, image->data_size, IPC_CREAT | 0777);
	if (memhandle >= 0) {
	  void *shmem = shmat(memhandle,NULL,0);
	  if (shmem && shmem != (void *)-1) { // Who created this interface?
	    image->data      = (char *)shmem;
	    shminfo.shmid    = memhandle;
	    shminfo.shmaddr  = (char *)shmem;
	    shminfo.readOnly = False;
	    // Ready to attach to the shared memory.
	    if (XShmAttach(d,&shminfo)) {
	      // Done and ready to use. Create a context. No special flags required.
	      context = XCreateGC(d,win,0,NULL);
	      if (context) {
		return; 
	      }
	      XShmDetach(d,&shminfo);
	    }
	    // Detach the segment, not usable.
	    shmdt(shmem);
	  }
	  // Delete the key.
	  shmctl(memhandle,IPC_RMID,NULL);
	}
	XFree(image);
      }
      XvUngrabPort(display,port,CurrentTime);
    }
    Throw(ObjectDoesntExist,"XvVideoShMem::XvVideoShMem",
	  "Unable to create shared memory segment for XVideo");
  }
  //
  ~XVideoShMem(void)
  {
    XFreeGC(display,context);
    //
    XShmDetach(display,&shminfo);
    // Detach the segment.
    shmdt(shmem);
    // Delete the key.
    shmctl(memhandle,IPC_RMID,NULL);
    // Release the image.
    XFree(image);
    // Release the port.
    XvUngrabPort(display,port,CurrentTime);
  }
};
///

/// X11_XVideoBuffer::X11_XVideoBuffer
X11_XVideoBuffer::X11_XVideoBuffer(class Machine *mach, class XFront *front)
  : X11_DisplayBuffer(mach,front), VideoMem(NULL),
    indexdirty(false), enforcefullrefresh(true),
    active(NULL), last(NULL), row(NULL), 
    idxactive(NULL), idxrow(NULL)
{ 
}
///

/// X11_XVideoBuffer::~X11_XVideoBuffer
X11_XVideoBuffer::~X11_XVideoBuffer(void)
{
  DetachFromX();
  // Delete the scanbuffers
  delete[] active;
  delete[] last;
  delete[] idxactive;  
}
///

/// X11_XVideoBuffer::DetachFromX
// Unlink the display buffer from the X system
void X11_XVideoBuffer::DetachFromX(void)
{
  delete VideoMem;VideoMem = NULL;
  //  
  // Perform this on the super class
  CloseX();
}
///



/// X11_XVideoBuffer::RebuildScreen
// Rebuild the screen. If differential is set, then only an incremental
// update to the last screen is made. Otherwise, a full screen
// rebuild is made.
void X11_XVideoBuffer::RebuildScreen(bool differential)
{
  int x,y;
  PackedRGB *row,*entry;
  PackedRGB *lastrow,*lastentry;
  char *target,*yuv;
  int   tmodulo;
  bool  differs = false;
  int   yoff = 0,uoff = 0;
  
  if (active == NULL || display == 0 || mapped == false || VideoMem == NULL)
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
  //
  // Get the pointer to the first row we want to render on the screen.
  row     = active + LeftEdge + TopEdge * modulo;
  lastrow = last   + LeftEdge + TopEdge * modulo;

  target  = VideoMem->image->data;
  tmodulo = VideoMem->image->width << 1;
  //
  if (enforcefullrefresh || last == NULL) {
    differential = false;
  }
  //
  switch(VideoMem->format) {
  case YUY2:
    yoff   = 0;
    uoff   = 1;
    break;
  case UYVY:
    yoff   = 1;
    uoff   = 0;
    break;
  }
  //
  for(y = 0;y < Height;y++,row += modulo,lastrow += modulo,target += tmodulo) {
    // Now get pointer to the start of the line
    entry       = row;
    lastentry   = lastrow;
    yuv         = target;
    //
    // Now go for all points in this scanline
    for(x = 0;x < Width;x++,entry++,lastentry++) {
      //
      if (differential == false || *entry != *lastentry) {
	float y,u,v;
	UBYTE r = (*entry >> 16);
	UBYTE g = (*entry >>  8);
	UBYTE b = (*entry >>  0);
	UBYTE yb;
	UBYTE ub,vb;
	//
	differs    = true;
	if (last)
	  *lastentry = *entry;
	y  = 0.299 * r + 0.587 * g + 0.114 * b;
	u  = (b - y) * 0.493;
	v  = (r - y) * 0.877;
	yb = (y > 0.0)?((y < 255.0)?UBYTE(y):255):(0);
	ub = 128 + ((u > -128.0)?((u < 127.0)?BYTE(u):127):(-128));
	vb = 128 + ((v > -128.0)?((v < 127.0)?BYTE(v):127):(-128));
	yuv[yoff]   = yuv[yoff+2] = yb;
	yuv[uoff]   = ub;
	yuv[uoff+2] = vb;
      }
      yuv += 4;
    }
  }
  //
  
  if (differs)
    XvShmPutImage(VideoMem->display,VideoMem->port,VideoMem->window,
		  VideoMem->context,VideoMem->image,
		  0,0,VideoMem->width,VideoMem->height,
		  0,0,Width * PixelWidth,Height * PixelHeight,
		  False);
  
  if (last)
    enforcefullrefresh = false;
}
///

/// X11_XVideoBuffer::NextBuffer
// Return the next active buffer and swap buffers
UBYTE *X11_XVideoBuffer::NextBuffer(void)
{
  PackedRGB *tmp;

  tmp    = active;
  active = last;
  last   = tmp;
  //
  // Check whether we have an active buffer. If not, we have to build it now.
  if (active == NULL || idxactive == NULL) {
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
    // Keep the modulo to ensure the refresh is right.
    modulo      = w;
  }
  
  // Reset the scan counter as well.
  row     = active;
  idxrow  = idxactive;
  lastrow = last;
  return idxactive;
}
///

/// X11_XVideoBuffer::DumpScreen
// Make a screen dump as .ppm file
void X11_XVideoBuffer::DumpScreen(FILE *file,ScreenDump::GfxFormat format)
{  
  class ScreenDump dumper(machine,machine->GTIA()->ActiveColorMap(),
			  LeftEdge,TopEdge,Width,Height,modulo,format);
  
  // Just use the dumper to get the jump done.
  if (active) {
    dumper.Dump(active,file);
  }
}
///

/// X11_XVideoBuffer::HandleExposure
// In case of a window exposure event, try to rebuild the window
// contents. This works only if we use an indirect drawing method
// using a pixmap. Otherwise, we ignore the event as we refresh
// the screen next time anyhow.
void X11_XVideoBuffer::HandleExposure(void)
{
  mapped             = true;  // the window is now available. (initial exposure)
  enforcefullrefresh = false;
  if (VideoMem) {
    XvShmPutImage(VideoMem->display,VideoMem->port,VideoMem->window,
		  VideoMem->context,VideoMem->image,
		  0,0,VideoMem->width,VideoMem->height,
		  0,0,Width * PixelWidth,Height * PixelHeight,
		  False);
  }
}
///

/// X11_XVideoBuffer::ConnectToX
// Connect to the X11 window system
void X11_XVideoBuffer::ConnectToX(Display *d,Screen *s,Window win,Colormap cm,
				  LONG le,LONG te,LONG w,LONG h,
				  LONG pxwidth,LONG pxheight,bool indirect)
{  
#if CHECK_LEVEL > 2
  if (pixmap || pixmapcontext || VideoMem) {
    Throw(ObjectExists,"X11_XVideoBuffer::ConnectToX",
	  "The display buffer is already connected to the X system");
  }
#endif
  //
  enforcefullrefresh = true;  // ensure that the next redraw is a complete redraw
  //
  // Get window, colormap, screen, display: All this in the
  // super class.
  SetupX(d,s,win,cm,le,te,w,h,pxwidth,pxheight,indirect);
  //
  // Check whether the shared memory extension is available
  if (!XShmQueryExtension(d))
    Throw(ObjectDoesntExist,"X11_XVideoBuffer::ConnectToX",
	  "The shared memory extension is not available, cannot create xvideo overlay");
  //
  // Check whether the xvideo extension is available.
  {
    unsigned int version,revision,request_base,event_base,error_base;
    unsigned int num_adaptors,i;
    XvAdaptorInfo *ai = NULL;
    //
    if (XvQueryExtension(d,&version,&revision,&request_base,&event_base,&error_base) != Success) {
      Throw(ObjectDoesntExist,"X11_XVideoBuffer::ConnectToX",
	    "The xvideo extension is not available, cannot create xvideo overlay");
    }
    if (version < 2 || (version == 2 && revision < 2)) 
      Throw(ObjectDoesntExist,"X11_XVideoBuffer::ConnectToX",
	    "The xvideo extension is too old, requires at least version 2.2");
    //
    // Check for the adaptors.
    XvQueryAdaptors(d,win,&num_adaptors,&ai);
    //
    // Find an adaptor that is input and image..
    for(i = 0;i < num_adaptors;i++) {
      if ((ai[i].type & XvInputMask) && (ai[i].type & XvImageMask)) {
	unsigned long k;
	XvPortID port;
	for(k = 0;k < ai[i].num_ports;k++) {
	  XvImageFormatValues *formats;
	  int f,num_formats;
	  //
	  // Get the next available port.
	  port = ai[i].base_id + k;
	  // Go through all the formats we support.
	  formats = XvListImageFormats(d, port, &num_formats);
	  if (formats) {
	    for(f = 0;f < num_formats;f++) {
	      // Only interested in packed formats.
	      if (formats[f].format == XvPacked) {
		if (formats[f].id == YUY2 || formats[f].id == UYVY) {
		  try {
		    VideoMem = new struct XVideoShMem(d,win,port,formats[f].id,w << 1,h);
		    return;
		  } catch(...) {
		    // Iterate on.
		  }
		}
	      }
	    } // Over possible formats.
	  }
	}
      }
    } // over adaptors.
  } // over video extension available
  //
  Throw(ObjectDoesntExist,"X11_XVideoBuffer::ConnectToX",
	"Found no suitable xvideo port or format to connect to");
}
///

/// X11_XVideoBuffer::ActiveBuffer
// Return the currently active screen buffer
UBYTE *X11_XVideoBuffer::ActiveBuffer(void)
{
  // In case we do not yet have a buffer, generate one
  if (idxactive == NULL || active == NULL) {
    return NextBuffer();
  }
  return idxactive;
}
///

/// X11_XVideoBuffer::ColdStart
// Run a power-up of the display buffer here.
void X11_XVideoBuffer::ColdStart(void)
{ 
  // Delete the screen buffers
  delete[] active;
  delete[] last;
  active      = NULL;
  last        = NULL;
  //
  // This also implies a warmstart
  WarmStart();
}
///

/// X11_XVideoBuffer::WarmStart
// Run a regular reset of the display buffer
void X11_XVideoBuffer::WarmStart(void)
{  
  enforcefullrefresh = true;  // ensure that the next redraw is a complete redraw
}
///

/// X11_XVideoBuffer::DisplayStatus
void X11_XVideoBuffer::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("X11_XVideoBuffer status:\n"
		   "\tPixel width           : " ATARIPP_LD "\n"
		   "\tPixel height          : " ATARIPP_LD "\n"
		   "\tTrue Color Renderer   : on\n"
		   "\tLeftEdge              : " ATARIPP_LD "\n"
		   "\tTopEdge               : " ATARIPP_LD "\n"
		   "\tWidth                 : " ATARIPP_LD "\n"
		   "\tHeight                : " ATARIPP_LD "\n",
		   PixelWidth,PixelHeight,
		   LeftEdge,TopEdge,Width,Height);
}
///

/// X11_XVideoBuffer::PushLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void X11_XVideoBuffer::PushLine(UBYTE *in,int size)
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

/// X11_XVideoBuffer::PushRGBLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void X11_XVideoBuffer::PushRGBLine(PackedRGB *,int)
{  
  row     += modulo;
  idxrow  += modulo;
}
///

/// X11_XVideoBuffer::ResetVertical
// Signal that we start the display generation again from top. Hence, this implements
// some kind of "vertical sync" signal for the display generation.
void X11_XVideoBuffer::ResetVertical(void)
{ 
  idxrow   = ActiveBuffer();
  row      = active;
  lastrow  = last;
}
///

/// X11_XVideoBuffer::SignalRect
// Signal the requirement for refresh in the indicated region. This is only
// used by the GUI engine as the emulator core uses PushLine for that.
// Not required here... X11 will find out itself.
void X11_XVideoBuffer::SignalRect(LONG,LONG,LONG,LONG)
{
  indexdirty = true;
}
///

///
#endif
#endif // of if HAVE_LIBX11
///
