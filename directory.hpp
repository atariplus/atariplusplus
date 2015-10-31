/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: directory.hpp,v 1.9 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Os compatibility layer for directory reading.
 ** This file takes definitions from "types.h" build by autoconf/configure
 ** and provides suitable wrapper functions for directory scanning 
 ** in case the host operating system does not implement them fully.
 **********************************************************************************/

#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP

/// Includes
#include "types.h"
#include "string.hpp"
///

/// Dirent replacements
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
inline static const char *de_name(struct dirent *d)
{
  return d->d_name;
}
#else
# ifdef _WIN32
struct DIR;
struct dirent;
extern DIR *opendir(const char *name);
extern struct dirent *readdir(DIR *dir);
extern int closedir(DIR *dir);
extern int NAMLEN(struct dirent *);
extern const char *de_name(struct dirent *d);
#define S_IWUSR S_IWRITE
# else
#  define dirent direct
#  define NAMLEN(dirent) (dirent)->d_namlen
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
extern const char *de_name(struct dirent *d);
# endif
#endif
///

/// stat replacements
#if HAVE_STAT_H || HAVE_SYS_STAT_H
#if HAVE_STAT_H
#include <stat.h>
#elif HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifndef S_ISDIR
#define S_ISDIR(x) (x & S_IFDIR)
#endif
#else              
// Define the stat buffer manually and fake it to "all regular files" (yuck)
// and define some of the macros we need
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IWUSR 0000200
// Check whether the file is a directory. Well, we cannot tell and thread everything as
// a plain file. Yuck.
#define S_ISDIR(x) (0)
struct stat {
  int        st_mode;     /* protection       */
  int        st_size;     /* size of the file */
};

inline static int stat(const char *file_name, struct stat *buf)
{
  buf->st_mode = S_IFREG; // regular file
  buf->st_size = 0;
  return 0;
}
//
// Provide a dummy chmod. Maybe the host operating system doesn't provide
// protection modes? If not, then we just ignore this here and don't bother.
#define chmod(path,mode) (0)
//
#endif
///

///
#endif
