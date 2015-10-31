/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filelist.hpp,v 1.4 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of a gadget representing a list of files to
 ** choose from, i.e. the basic ingredience for a file requester.
 **********************************************************************************/

#ifndef FILELIST_HPP
#define FILELIST_HPP

/// Includes
#include "gadget.hpp"
#include "directory.hpp"
#include "stringgadget.hpp"
#include "buttongadget.hpp"
#include "verticalgroup.hpp"
///

/// Class FileList
// A gadget presenting a list of files to select from, plus all the context to build them.
// We do not derive the vertical group directly as we need to construct it with a custom
// render port.
class FileList : public Gadget {
  //
  // The temporary directory handle
  DIR                     *DirHandle;
  //
  // List of internal gadgets that are not part of the vertical group since we do not
  // want to scroll them.
  List<Gadget>             InternalGadgets;
  //
  // private renderport for clipping the directory contents into.
  class RenderPort        *ClipRegion;
  //
  // Buttons for the file name to be added to the list.
  class StringGadget      *PathGadget;
  class ButtonGadget      *OKButton;
  class ButtonGadget      *CancelButton;
  class VerticalGroup     *Directory;
  //
  // If the following is set to true, then we're requesting a directory.
  bool                     DirsOnly;
  // If the following is set to true, then we do not allow the selection of a directory as
  // final element.
  bool                     FilesOnly;
  // If the following is set to true, also non-existing entries will be accepted because
  // we want to save into something
  bool                     ForSave;
  //
  // This is set to the active gadget.
  class Gadget            *ActiveGadget;
  //
  // A temporary string we operate on for all the string operations.
  char                    *TmpPath;
  char                    *StatPath;
  //
  // Read the contents of the directory given by the contents of the directory
  // whose name is in the path gadget.
  void ReadDirectory(void);
  //
  // Attach the given string to the string stored in the path gadget. This either enters a
  // directory, or goes up one directory, or just accepts a file name. Details depend on the
  // type we're working in.
  void AttachPath(const char *add);
  //
  // Check whether the given path represents a directory like entry.
  // Returns true if so.
  static bool IsDirectory(const char *name);
  //
  // Check whether the given path represents a file like entry.
  // Returns true if so. This is called to decide whether we have to remove a trailing "/", so
  // be gracious and handle "special files" like regular ones.
  static bool IsFile(const char *name);
  //
  // Return a pointer to the character that separates the directory name from the file part.
  static char *PathPart(char *name);
  //
public:
  // Construct a file list. This requires also an initial path, and a flag whether
  // we accept only directories.
  FileList(List<Gadget> &gadgetlist,
	   class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
	   const char *initial,
	   bool save = false,bool filesonly = true,bool dirsonly = false);
  virtual ~FileList(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Refresh this gadget and all gadgets inside.
  virtual void Refresh(void);
  //
  // Return the currently selected list
  const char *GetStatus(void)
  {
    return PathGadget->GetStatus();
  }
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  virtual const class Gadget *FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const;
};
///

///
#endif
