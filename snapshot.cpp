/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: snapshot.cpp,v 1.4 2015/05/21 18:52:43 thor Exp $
 **
 ** This class defines an interface that loads and saves state configurations
 ** to an (possibly external) source.
 ** It is an extension of the argument parser that also allows to save entire
 ** blocks (e.g. memory pages) to an external source.
 **********************************************************************************/

/// Includes
#include "argparser.hpp"
#include "snapshot.hpp"
///

/// SnapShot::SnapShot
// A snaphshot class initializes the argparser class in a way
// that never signals the requirement to generate help output.
// It is simply not required.
SnapShot::SnapShot(void)
  : ArgParser(false)
{
}
///

/// SnapShot::SnapShot
SnapShot::~SnapShot(void)
{
}
///

/// SnapShot::PrintHelp
// The following methods from the argparser class are no longer public
// because they don't make sense here. We overload them with dummies.
// Print help text over the required output stream.
void SnapShot::PrintHelp(const char *,...)
{
}
///

/// SnapShot::SignalBigChange
// Signal a change in the argument change flag, i.e. prepare to re-read
// some arguments if this is required. This method is here for client
// purposes that may require to enforce an argument re-parsing.
// This is a dummy here since loading a snapshot is always a bigchange.
void SnapShot::SignalBigChange(ArgumentChange)
{
}
///
