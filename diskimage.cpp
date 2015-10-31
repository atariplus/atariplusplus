/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: diskimage.cpp,v 1.3 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Abstract base class for disk images
 **********************************************************************************/

/// Includes
#include "diskimage.hpp"
///

/// DiskImage::DiskImage
DiskImage::DiskImage(class Machine *mach)
  : Machine(mach)
{
}
///

/// DiskImage::~DiskImage
DiskImage::~DiskImage(void)
{
}
///
