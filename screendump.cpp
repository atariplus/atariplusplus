/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: screendump.cpp,v 1.13 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Creation of screendumps into a file
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "screendump.hpp"
#include "exceptions.hpp"
#include "machine.hpp"
#include "string.hpp"
#include "stdio.hpp"
#ifdef USE_PNG
#include <png.h>
#endif
///

/// ScreenDump::ScreenDump
ScreenDump::ScreenDump(class Machine *mach,
		       const struct ColorEntry *colors,
		       LONG leftedge,LONG topedge,LONG width,LONG height,LONG modulo,
		       GfxFormat format)
  : machine(mach), colormap(colors),
    LeftEdge(leftedge), TopEdge(topedge),
    Width(width), Height(height), Modulo(modulo),
    Format(format)
{ 
#ifdef USE_PNG
  png_ptr     = NULL;
  info_ptr    = NULL;
  png_palette = NULL;
  rowbuffer   = NULL;
#endif
}
///

/// ScreenDump::~ScreenDump
// Cleanup a screen dump,dispose temporaries.
ScreenDump::~ScreenDump(void)
{
#ifdef USE_PNG
  delete[] png_palette;
  delete[] rowbuffer;
  if (png_ptr) {
    png_destroy_write_struct(&png_ptr,&info_ptr);
  }
#endif
}
///

/// ScreenDump::Dump
// Dump the screen contents 
void ScreenDump::Dump(UBYTE *buffer,FILE *file)
{
  if (buffer == NULL) {
    Throw(ObjectDoesntExist,"ScreenDump::Dump",
	  "There is no screen to create a dump of");
  }
  //
  switch(Format) {
  case PNM:
    DumpPNM(buffer,file);
    break;
  case BMP:
    DumpBMP(buffer,file);
    break;
#ifdef USE_PNG
  case PNG:
    DumpPNG(buffer,file);
    break;
#endif
  default:
    Throw(InvalidParameter,"ScreenDump::Dump","unsupported gfx format requested");
  }
}
///

/// ScreenDump::Dump
// Dump the true color screen contents 
void ScreenDump::Dump(PackedRGB *buffer,FILE *file)
{
  if (buffer == NULL) {
    Throw(ObjectDoesntExist,"ScreenDump::Dump",
	  "There is no screen to create a dump of");
  }
  //
  switch(Format) {
  case PNM:
    DumpPNM(buffer,file);
    break;
  case BMP:
    DumpBMP(buffer,file);
    break;
#ifdef USE_PNG
  case PNG:
    DumpPNG(buffer,file);
    break;
#endif
  default:
    Throw(InvalidParameter,"ScreenDump::Dump","unsupported gfx format requested");
  }
}
///

/// ScreenDump::BMPHeader::BMPHeader
// Initialize a BMP header structure for the
// given width and height.
ScreenDump::BMPHeader::BMPHeader(ULONG width,ULONG height,UBYTE bpp)
{ 
  ULONG size;
  ULONG cludgefill;
  ULONG offset;
  ULONG typesize;
  //  
  // Cludgefill to round the width up to a multiple of 32 bit.
  switch(bpp) {
  case 8:
    cludgefill  = ((width + 3) & (~3)) - width;
    size        = (cludgefill + width) * height + sizeof(*this) + 256 * 4;
    break;
  case 24:
    cludgefill  = (((width * 3) + 3) & (~3)) - width;
    size        = (cludgefill + width * 3) * height + sizeof(*this) + 256 * 4;
    break;
  default:
    Throw(InvalidParameter,"ScreenDump::BMPHeader::BMPHeader","unsupported bit depth");
    break;
  }
  //
  // Compute the resulting file size: Header, plus image data, 
  // plus size of the palette
  offset        = sizeof(*this) + 256 * 4;
  typesize      = sizeof(*this) - 14;
  //
  // Fill in the BMP header now (this is the new BMP header)
  memset(this,0,sizeof(*this));
  //
  id0           = 'B';
  id1           = 'M'; // magic cookie
  // Fill in the file size in LE
  BufSize[0]    = UBYTE(size);
  BufSize[1]    = UBYTE(size >> 8);
  BufSize[2]    = UBYTE(size >> 16);
  BufSize[3]    = UBYTE(size >> 24);
  OffsetBits[0] = UBYTE(offset);
  OffsetBits[1] = UBYTE(offset >> 8);
  OffsetBits[2] = UBYTE(offset >> 16);
  OffsetBits[3] = UBYTE(offset >> 24);
  TypeSize[0]   = UBYTE(typesize);
  TypeSize[1]   = UBYTE(typesize >> 8);
  TypeSize[2]   = UBYTE(typesize >> 16);
  TypeSize[3]   = UBYTE(typesize >> 24);
  Width[0]      = UBYTE(width);
  Width[1]      = UBYTE(width >> 8);
  Width[2]      = UBYTE(width >> 16);
  Width[3]      = UBYTE(width >> 24);
  Height[0]     = UBYTE(height);
  Height[1]     = UBYTE(height >> 8);
  Height[2]     = UBYTE(height >> 16);
  Height[3]     = UBYTE(height >> 24);
  Planes[0]     = 1;   // A single palette plane
  BitCount[0]   = bpp; // bpp
  ClrU[1]       = UBYTE(256 >> 8); // entries to be reserved for the palette: 256
  Imp[1]        = UBYTE(256 >> 8);
}
///

/// ScreenDump::DumpBMP
// Dump the screen as BMP image
void ScreenDump::DumpBMP(UBYTE *buffer,FILE *file)
{
  ULONG cludgefill;
  UBYTE rgb[4];
  int i;
  LONG y;
  UBYTE *row;
  // The BMP header  
  struct BMPHeader hdr(Width,Height,8);
  //
  // Cludgefill to round the width up to a multiple of 32 bit.
  cludgefill = ((Width + 3) & (~3)) - Width;
  //
  // Now write this to the file.
  if (fwrite(&hdr,1,sizeof(hdr),file) != sizeof(hdr)) {
    ThrowIo("ScreenDump::DumpBMP","Failed to write the BMP header");
  }
  //
  // Write now the palette in BGR (wierd Win-World!)
  for(i=0;i<256;i++) {
    rgb[0] = colormap[i].blue;
    rgb[1] = colormap[i].green;
    rgb[2] = colormap[i].red;
    rgb[3] = 0;
    if (fwrite(rgb,1,4,file) != 4) {
      ThrowIo("ScreenDump::DumpBMP","Failed to write the palette");
    }
  }
  //
  // Now write the image, but downside up.
  row = buffer + LeftEdge + (TopEdge + Height) * Modulo;
  // Mis-use the color buffer as cludge-fill source
  memset(rgb,0,sizeof(rgb));
  for(y = 0; y < Height; y++) {
    row -= Modulo;
    // Write the row out.
    if (fwrite(row,1,Width,file) != size_t(Width)) {
      ThrowIo("ScreenDump::DumpBMP","Failed to write a bitmap row");
    }
    // Write out cludge fill to make the width a multiple
    // of 32 bits wide.
    if (cludgefill) {
      if (fwrite(rgb,1,cludgefill,file) != cludgefill) {
	ThrowIo("ScreenDump::DumpBMP","Failed to write the cludge fill zeros");
      }
    }
  }
}
///

/// ScreenDump::DumpBMP
// Dump the screen as BMP image
void ScreenDump::DumpBMP(PackedRGB *buffer,FILE *file)
{
  ULONG cludgefill;
  UBYTE rgb[4];
  int i;
  LONG y;
  PackedRGB *row;
  // The BMP header 
  struct BMPHeader hdr(Width,Height,24);
  //
  // Cludgefill to round the width up to a multiple of 32 bit.
  cludgefill = ((Width + 3) & (~3)) - Width;
  //
  // Now write this to the file.
  if (fwrite(&hdr,1,sizeof(hdr),file) != sizeof(hdr)) {
    ThrowIo("ScreenDump::DumpBMP","Failed to write the BMP header");
  }
  //
  // Write now the palette in BGR (wierd Win-World!)
  for(i=0;i<256;i++) {
    rgb[0] = colormap[i].blue;
    rgb[1] = colormap[i].green;
    rgb[2] = colormap[i].red;
    rgb[3] = 0;
    if (fwrite(rgb,1,4,file) != 4) {
      ThrowIo("ScreenDump::DumpBMP","Failed to write the palette");
    }
  }
  //
  // Now write the image, but downside up.
  row = buffer + LeftEdge + (TopEdge + Height) * Modulo;
  // Mis-use the color buffer as cludge-fill source
  memset(rgb,0,sizeof(rgb));
  for(y = 0; y < Height; y++) {
    LONG x;
    row -= Modulo;
    // Write the row out. Since we want to be endian-independent, extract
    // manually as BGR and write pixel by pixel.
    for(x = 0; x < Width;x++) {
      rgb[0] = UBYTE((row[x] >>  0) & 0xff); // blue
      rgb[1] = UBYTE((row[x] >>  8) & 0xff); // green
      rgb[2] = UBYTE((row[x] >> 16) & 0xff); // red
      if (fwrite(rgb,1,3,file) != 3) {
	ThrowIo("ScreenDump::DumpBMP","Failed to write a bitmap row");
      }
    }
    // Write out cludge fill to make the width a multiple
    // of 32 bits wide.
    if (cludgefill) {
      if (fwrite(rgb,1,cludgefill,file) != cludgefill) {
	ThrowIo("ScreenDump::DumpBMP","Failed to write the cludge fill zeros");
      }
    }
  }
}
///

/// ScreenDump::DumpPNM
// Dump the screen contents as PNM (actually, PPM) 
// format.
void ScreenDump::DumpPNM(UBYTE *buffer,FILE *file)
{
  UBYTE rgb[3];
  UBYTE *p;
  int x,y;

  fprintf(file,"P6\n" ATARIPP_LD " " ATARIPP_LD "\n255\n",Width,Height);
  p = buffer + LeftEdge + TopEdge * Modulo;
  for(y = 0;y<Height;y++) {
    for(x = 0;x<Width;x++) {
      rgb[0] = colormap[*p].red;
      rgb[1] = colormap[*p].green;
      rgb[2] = colormap[*p].blue;
      if (fwrite(rgb,sizeof(UBYTE),3,file) != 3) {
	ThrowIo("ScreenDump::DumpPNM",
		"Failed to create the screen dump");
      }
      p++;
    }
    p += Modulo - Width;
  }
}
///

/// ScreenDump::DumpPNM
// Dump the screen contents as PNM (actually, PPM) 
// format.
void ScreenDump::DumpPNM(PackedRGB *buffer,FILE *file)
{
  UBYTE rgb[3];
  PackedRGB *p;
  int x,y;

  fprintf(file,"P6\n" ATARIPP_LD " " ATARIPP_LD "\n255\n",Width,Height);
  p = buffer + LeftEdge + TopEdge * Modulo;
  for(y = 0;y<Height;y++) {
    for(x = 0;x<Width;x++) {
      rgb[0] = UBYTE((*p >> 16) & 0xff);
      rgb[1] = UBYTE((*p >>  8) & 0xff);
      rgb[2] = UBYTE((*p >>  0) & 0xff);
      if (fwrite(rgb,sizeof(UBYTE),3,file) != 3) {
	ThrowIo("ScreenDump::DumpPNM",
		"Failed to create the screen dump");
      }
      p++;
    }
    p += Modulo - Width;
  }
}
///

/// ScreenDump::DumpPNG
// Write a screen dump as PNG file to disk
#ifdef USE_PNG
void ScreenDump::DumpPNG(UBYTE *buffer,FILE *file)
{
  LONG i;
  png_bytep row_pointer;
  
  // Create the PNG structure
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,this,&PNGErrorHandler,&PNGWarningHandler);
  if (png_ptr == NULL) {
    Throw(NoMem,"ScreenDump::DumpPNG","no memory for the PNG structure");
  }
  // Create the PNG info structure
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    Throw(NoMem,"ScreenDump::DumpPNG","no memory for the PNG info structure");
  }
  // No error case here. Pass the IO function as IO stream.
  png_init_io(png_ptr,file);
  // Fill in the image characteristics for PNG now. Width, Height are
  // from the outside, depth is 8bpp, we write palette images for
  // better performance.
  png_set_IHDR(png_ptr,info_ptr,Width,Height,8,PNG_COLOR_TYPE_PALETTE,
	       PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,
	       PNG_FILTER_TYPE_DEFAULT);
  // Allocate and fill-in the palette.
  png_palette = new png_color[256];
  for(i=0;i<256;i++) {
    png_palette[i].red   = colormap[i].red;
    png_palette[i].green = colormap[i].green;
    png_palette[i].blue  = colormap[i].blue;
  }
  // Install this as the palette
  png_set_PLTE(png_ptr,info_ptr,png_palette,256);
  // Write all the informtion into the stream now.
  png_write_info(png_ptr,info_ptr);
  // Write now the image one row after another. 
  row_pointer = buffer + LeftEdge + TopEdge * Modulo;
  for(i=0;i<Height;i++) {
    png_write_row(png_ptr,row_pointer);
    row_pointer += Modulo;
  }
  // Finish writing, do not pass the info_ptr again.
  png_write_end(png_ptr,NULL);
  // Other cleanup is done by the destructor.
}
#endif
///

/// ScreenDump::DumpPNG
// Write a screen dump as PNG file to disk
#ifdef USE_PNG
void ScreenDump::DumpPNG(PackedRGB *buffer,FILE *file)
{
  LONG i;
  LONG x;
  PackedRGB *row_pointer;
  // Create a row buffer large enough to contain the colors of one row.
  delete[] rowbuffer;
  rowbuffer  = NULL;
  rowbuffer  = new png_byte[3*Width];
  //
  // Create the PNG structure
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,this,&PNGErrorHandler,&PNGWarningHandler);
  if (png_ptr == NULL) {
    Throw(NoMem,"ScreenDump::DumpPNG","no memory for the PNG structure");
  }
  // Create the PNG info structure
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    Throw(NoMem,"ScreenDump::DumpPNG","no memory for the PNG info structure");
  }
  // No error case here. Pass the IO function as IO stream.
  png_init_io(png_ptr,file);
  // Fill in the image characteristics for PNG now. Width, Height are
  // from the outside, depth is 24bpp.
  png_set_IHDR(png_ptr,info_ptr,Width,Height,8,PNG_COLOR_TYPE_RGB,
	       PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,
	       PNG_FILTER_TYPE_DEFAULT);
  // Write all the informtion into the stream now.
  png_write_info(png_ptr,info_ptr);
  // Write now the image one row after another. 
  row_pointer = buffer + LeftEdge + TopEdge * Modulo;
  //
  // Now compress the image.
  for(i=0;i<Height;i++) {
    // Now extract one row and convert it from packedRGB into individual
    // RGB bytes. For all the smartasses: Yes, this is necessary since
    // we want to be endian-independent...
    png_byte *fillin = rowbuffer;
    PackedRGB *src   = row_pointer;
    for(x=0;x<Width;x++) {
      *fillin++ = (*src >> 16) & 0xff; // red
      *fillin++ = (*src >>  8) & 0xff; // green
      *fillin++ = (*src >>  0) & 0xff; // blue;
      src++;
    }
    png_write_row(png_ptr,rowbuffer);
    row_pointer += Modulo;
  }
  // Finish writing, do not pass the info_ptr again.
  png_write_end(png_ptr,NULL);
  // Other cleanup is done by the destructor.
  delete[] rowbuffer;
  rowbuffer = NULL;
}
#endif
///

/// PNGErrorHandler
// PNG specific error and warning callback entries.
#ifdef USE_PNG
void ScreenDump::PNGErrorHandler(png_structp,png_const_charp error_msg)
{
  // Throw an exception here.
  Throw(IoErr,"ScreenDump::PNGErrorHandler",error_msg);
}
#endif
///

/// PNGWarningHandler
// PNG specific warning callback entry
#ifdef USE_PNG
void ScreenDump::PNGWarningHandler(png_structp png_ptr,png_const_charp error_msg)
{
  class ScreenDump *that = (class ScreenDump *)png_get_error_ptr(png_ptr);
  //
  // Use the machine PutWarning for that.
  that->machine->PutWarning("%s",(const char *)error_msg);
}
#endif
///
