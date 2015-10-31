/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: screendump.hpp,v 1.10 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Creation of screendumps into a file
 **********************************************************************************/

#ifndef SCREENDUMP_HPP
#define SCREENDUMP_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "display.hpp"
#include "colorentry.hpp"
#if HAVE_PNG_CREATE_INFO_STRUCT && HAVE_PNG_CREATE_WRITE_STRUCT && HAVE_PNG_H && HAVE_PNG_WRITE_CHUNK
#define USE_PNG
#include <png.h>
#endif
///

/// Forwards
class Machine;
///

/// Class ScreenDump
// This class implements a screen-dump feature
class ScreenDump {  
public:
  // Definition of various gfx formats we can support.
  enum GfxFormat {
#ifdef USE_PNG
    PNG,                   // advanced: PNG image format
#endif
    BMP,                   // simple BMP image
    PNM                    // default: Simplicistic PNM
  };
  //
  // Format for true color graphics.
  typedef AtariDisplay::PackedRGB PackedRGB;
  //
private:
  //
  // Back-pointer to the machine for error handling
  class Machine *machine;
  //
  // The colormap defines how each atari color has to be mapped
  // to an RGB value.
  const struct ColorEntry *colormap;
  //
  // Dimensions of the screen, to be given on construction
  LONG           LeftEdge,TopEdge;
  LONG           Width,Height;
  LONG           Modulo;
  GfxFormat      Format;
  //
  // The header of a BMP file
  //  
  struct BMPHeader {
    UBYTE  id0,id1;       // must be B and M, respectively
    UBYTE  BufSize[4];    // buffer size = file size, in 
    // Little Endian. Not that we need it....
    UBYTE  res1[2];
    UBYTE  res2[2];        // currently not used
    UBYTE  OffsetBits[4];  // ULONG LE offset
    // The following is actually part of the nid or oid...
    // We keep it here for consistency.
    UBYTE  TypeSize[4];    // BMP encoding scheme
    UBYTE  Width[4];       // width, little endian
    UBYTE  Height[4];      // height, little endian
    UBYTE  Planes[2];      // # of planes. Must be one
    UBYTE  BitCount[2];
    UBYTE  Compress[4];    // compression type
    UBYTE  SizeImage[4];   // image size
    UBYTE  XPPM[4];        // X pels per meter
    UBYTE  YPPM[4];        // Y pels per meter
    UBYTE  ClrU[4]; 
    UBYTE  Imp[4];
    //
    // Construct the guy
    BMPHeader(ULONG width,ULONG height,UBYTE bpp);
  };
  //
  // Dumpers for various image formats
  void DumpPNM(UBYTE *buffer,FILE *file);
  void DumpPNM(PackedRGB *buffer,FILE *file);
  void DumpBMP(UBYTE *buffer,FILE *file);
  void DumpBMP(PackedRGB *buffer,FILE *file);
#ifdef USE_PNG
  void DumpPNG(UBYTE *buffer,FILE *file);
  void DumpPNG(PackedRGB *buffer,FILE *file);
  //
  // PNG specific error and warning callback entries.
  static void PNGErrorHandler(png_structp png_ptr,png_const_charp error_msg);
  static void PNGWarningHandler(png_structp png_ptr,png_const_charp error_msg);
  //
  // Various PNG specific state variables
  png_structp png_ptr;
  png_infop   info_ptr;
  // PNG palette information
  png_color  *png_palette;
  //
  // PNG row buffer for RGB images.
  png_byte   *rowbuffer;
  //
#endif
  //
public:
  ScreenDump(class Machine *mach,
	     const struct ColorEntry *colors,
	     LONG leftedge,LONG topedge,LONG width,LONG height,LONG modulo,
	     GfxFormat format = PNM);
  ~ScreenDump(void);
  //
  // Make a screen dump as .ppm file for palettized formats.
  void Dump(UBYTE *buffer,FILE *file);
  //
  // The same for true color graphics.
  void Dump(PackedRGB *buffer,FILE *file);
};
///

///
#endif

