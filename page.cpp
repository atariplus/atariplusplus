/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: page.cpp,v 1.12 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of an abstract page, keeping 256 in common
 **********************************************************************************/

/// Includes
#include "page.hpp"
#include "stdio.hpp"
///

/// Page::ReadFromFile
// Read a page from an external file
// This respects special access rules for the page
bool Page::ReadFromFile(FILE *file)
{
  int i;
  UBYTE buffer[256];

  if (fread(buffer,sizeof(UBYTE),256,file) != 256) {
    if (errno == 0) {
      errno = ERANGE;
    }
    return false;
  }

  for(i=0;i<256;i++) {
    PatchByte(i,buffer[i]);
  }
  return true;
}
///

/// Page::WriteToFile
// Write a page to an external file
// This respects special access rules for the page
bool Page::WriteToFile(FILE *file)
{
  int i;
  UBYTE buffer[256];

  for(i=0;i<256;i++) {
    buffer[i] = ReadByte(i);
  }

  if (fwrite(buffer,sizeof(UBYTE),256,file) != 256) 
    return false;

  return true;
}
///
