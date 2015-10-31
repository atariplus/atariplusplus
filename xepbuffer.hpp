/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xepbuffer.hpp,v 1.4 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: The character buffer of the XEP output
 **********************************************************************************/

#ifndef XEPBUFFER_HPP
#define XEPBUFFER_HPP

/// Includes
#include "types.hpp"
#include "chip.hpp"
#include "configurable.hpp"
#include "saveable.hpp"
///

/// Forwards
class AtariDisplay;
class Machine;
class Monitor;
class ArgParser;
class SnapShot;
///

/// class XEPBuffer
class XEPBuffer : public Chip, public Saveable {
  //
public:
  // Constants for the dimensions of the
  // display buffer.
  static const int    CharacterWidth  INIT(80);
  static const int    CharacterHeight INIT(25);
  //
private:
  //
  // The character buffer of the XEP output device.
  UBYTE              *Characters;
  //
  // The screen we render the characters to.
  class AtariDisplay *Screen;
  //
  // Raw graphics buffer of the currently active
  // screen.
  UBYTE              *RawBuffer;
  //
  // Layout of the raw buffer
  LONG                Width,Height,Modulo;
  //
  // Font used to render the contents. This is
  // currently the default 8x8 Atari font.
  const UBYTE        *Font;
  //
  // Preferences
  //
  // Front and back color for the text in the
  // color box. We separate this into color and hue
  // to be a bit more user friendly.
  LONG                FrontHue;
  LONG                FrontLuminance;
  LONG                BackHue;
  LONG                BackLuminance;
  //
  // Refresh the the rectangle to the output
  // display given the coordinates of the rectangle
  // in character coordinates.
  void RefreshRegion(LONG x,LONG y,LONG w,LONG h);
  //     
  //
public:
  // Construct and destruct this class.
  XEPBuffer(class Machine *mach);
  ~XEPBuffer(void);
  //
  // Warmstart and Coldstart of this class.
  virtual void WarmStart(void);
  virtual void ColdStart(void);
  //
  // Display the internal state of this class
  // for debugging purposes
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Configure this class.
  virtual void ParseArgs(class ArgParser *args);
  //
  // Save or load the internal state of this class.
  virtual void State(class SnapShot *snap);
};
///

///
#endif
