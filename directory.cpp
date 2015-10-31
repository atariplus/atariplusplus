/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: directory.cpp,v 1.13 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Os compatibility layer for directory reading.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions for directory scanning 
 ** in case the host operating system does not implement them fully.
 **********************************************************************************/

/// Includes
#include "directory.hpp"
#include "new.hpp"
///

/// Win32 directory functions
// with a home-made DIRectory functions.
# ifdef _WIN32
// win32 directory functions by JW are here
#  undef _POSIX_
#  include <io.h>
struct dirent {
  _finddata_t fdata;
};

struct DIR {
  intptr_t      handle;
  bool          bneedfirst;
  struct dirent d;
  char         *mask;
  //
  DIR(const char *name)
    : handle(0), bneedfirst(true), mask(new char[strlen(name) + 3])
  { 
    strcpy(mask,name);
    strcat(mask,"\\*");
  }
  //
  ~DIR(void)
  {
    delete[] mask;
    _findclose(handle);
  }
};

DIR *opendir(const char *name)
{
  struct DIR *d   = new struct DIR(name);
  d->handle      = _findfirst(d->mask,&(d->d.fdata));
  if (d->handle != -1) {
    return d;
  } else {
    delete d;
    return NULL;
  }
}

struct dirent *readdir(DIR *dir)
{
  if (dir->bneedfirst == false) {
    if (dir->handle == -1)
      return NULL;
    if (_findnext(dir->handle,&(dir->d.fdata))) {
      return NULL;
    }
  }
  dir->bneedfirst = false;
  return &(dir->d);
}

int closedir(DIR *dir)
{
  delete dir;
  return 0;
}

int NAMLEN(struct dirent *d)
{
  return (int)strlen(d->fdata.name);
}

const char *de_name(struct dirent *d)
{
  return d->fdata.name;
}
#endif
///

/// de_name for non-dirent
#if !HAVE_DIRENT_H && !defined(_WIN32)
const inline char *de_name(struct dirent *d)
{
  size_t l = NAMLEN(d);
  static char buf[256];
  
  if (l > 255) l = 255;
  memcpy(buf,d->name,l);
  buf[l] = '\0';
  
  return buf;
}
#endif
///
