/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filestream.cpp,v 1.8 2021/05/16 10:07:31 thor Exp $
 **
 ** In this module: Disk image source stream interface towards stdio
 **********************************************************************************/

/// Includes
#include "filestream.hpp"
#include "exceptions.hpp"
#include "stdio.hpp"
#include "directory.hpp"
///

/// FileStream::FileStream
FileStream::FileStream(void)
  : File(NULL), Size(0), IsProtected(false)
{ }
///

/// FileStream::~FileStream
// Cleanup the file stream now, close the file.
FileStream::~FileStream(void)
{
  if (File) {
    fclose(File);
  }
}
///

/// FileStream::OpenImage
// Open a disk image from a file name. Fill in
// the protection status and other nifties.
void FileStream::OpenImage(const char *name)
{
  struct stat info;
  LONG here;
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (File) {
    Throw(ObjectExists,"FileStream::OpenImage",
	  "the image has been opened already");
  }
#endif
  //
  // Get some information about the image file, e.g. wether it is
  // write-protected.
  if (stat(name,&info)) {
    ThrowIo("FileStream::OpenImage","unable to stat the image file");
  }
  if (info.st_mode & S_IFDIR) {
    Throw(InvalidParameter,"FileStream::OpenImage","image MUST be a file, not a directory");
  }
  if (info.st_mode & S_IWUSR) {
    // We may write to it.
    IsProtected = false;
    File        = fopen(name,"r+b");
    // Win doesn't always report the protection status correctly. Bummer!
    if (File == NULL) {
	IsProtected = true;
	File        = fopen(name,"rb");
    }
  } else {
    IsProtected = true;
    File        = fopen(name,"rb");
  }
  if (File == NULL)
    ThrowIo("FileStream::OpenImage","unable to open the input stream");
  //
  // Now find some characteristics, namely the file size.
  // For that, seek to the end of the file.
  if (fseek(File,0,SEEK_END)) {
    fclose(File);
    File = NULL;
    ThrowIo("FileStream::OpenImage","unable to seek in the image file");
  }
  // Now get the size of the image.
  here = ftell(File);
  if (here < 0) {
    fclose(File);
    File = NULL;
    ThrowIo("FileStream::OpenImage","unable to get the file pointer location in the image file");
  }
  Size = here;
}
///

/// FileStream::FormatImage
// Open an image for writing, leaving the file blank
// and the file size unlimited (for the time being).
bool FileStream::FormatImage(const char *filename)
{
  // First check whether we are already open. If so, we
  // cannot re-open again.
#if CHECK_LEVEL > 0
  if (File) {
    Throw(ObjectExists,"FileStream::FormatImage",
	  "the image has been opened already");
  }
#endif
  //
  File        = fopen(filename,"wb");
  if (File == NULL)
    return false;
  //
  // Set the size to unlimited
  Size = (ULONG)(~0);
  return true;
}
///

/// FileStream::Read
// Reads data from the image into the buffer from the given
// byte offset.
bool FileStream::Read(ULONG offset,void *buffer,ULONG size)
{
  //
#if CHECK_LEVEL > 0
  if (File == NULL)
    Throw(ObjectDoesntExist,"FileStream::Read","the image has not yet been opened");
#endif
  // Try to read past the end?
  if (offset + size > Size)
    return false;
  //
  // Ok, things seem sane here. Position the file pointer and read the data.
  if (fseek(File,offset,SEEK_SET))
    return false;
  //
  // Read the data now.
  if (fread(buffer,sizeof(UBYTE),size,File) != size)
    return false;
  return true;
}
///

/// FileStream::Write
// Write data back into the image at the given offset.
bool FileStream::Write(ULONG offset,const void *buffer,ULONG size)
{
  //
#if CHECK_LEVEL > 0
  if (File == NULL)
    Throw(ObjectDoesntExist,"FileStream::Read","the image has not yet been opened");
#endif
  // Check wether we may write first.
  if (IsProtected)
    return false;
  // Try to write past the end?
  if (offset + size > Size)
    return false;
  //
  // Ok, things seem sane here. Position the file pointer and read the data.
  if (fseek(File,offset,SEEK_SET))
    return false;
  //
  // write the data now.
  if (fwrite(buffer,sizeof(UBYTE),size,File) != size)
    return false;
  return true;
}
///
