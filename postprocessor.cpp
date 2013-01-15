/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: postprocessor.cpp,v 1.1 2013-01-12 11:06:00 thor Exp $
 **
 ** In this module: Display postprocessor base class
 **********************************************************************************/

/// Includes
#include "postprocessor.hpp"
#include "machine.hpp"
#include "colorentry.hpp"
#include "display.hpp"
///

/// PostProcessor::PostProcessor
// Setup the post processor base class.
PostProcessor::PostProcessor(class Machine *mach,const struct ColorEntry *colormap)
  : machine(mach), display(mach->Display()),
    ColorMap(colormap)
{
}
///

/// PostProcessor::~PostProcessor
// Dispose the postprocessor base class.
PostProcessor::~PostProcessor(void)
{
}
///

