/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: postprocessor.cpp,v 1.2 2015/05/21 18:52:41 thor Exp $
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

