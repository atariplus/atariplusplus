/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filerequester.hpp,v 1.4 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: A requester class that requests a file name from the user
 **********************************************************************************/

#ifndef FILEREQUESTER_HPP
#define FILEREQUESTER_HPP

/// Includes
#include "list.hpp"
#include "requester.hpp"
#include "event.hpp"
#include "listbrowsergadget.hpp"
///

/// Forwards
class ButtonGadget;
class TextGadget;
class Machine;
class FileList;
///

/// Class FileRequester
// This requester prints and logs warnings
class FileRequester : public Requester {
  //
  // The body of the requester. Contains it all.
  class FileList       *FileSelections; 
  //
  // Options that have been parsed in and that are kept here.
  // The initially selected file.
  const char          *InitialFile;
  // A boolean flag whether we are requesting for saving or loading
  bool                 Saving;
  // A boolean flag that is set if we are only asking for files
  bool                 FilesOnly;
  // A boolean flag that is set if we are only asking for directories
  bool                 DirsOnly;
  // The title of the menu. This gets displayed on top of the requester
  const char          *Title;
  //
  // The resulting file name is kept here. It must get copied before
  // the file list gets destroyed.
  char                *Result;
  //
  // Gadget build up and destroy routines.
  virtual void BuildGadgets(List<Gadget> &glist,class RenderPort *rport);
  virtual void CleanupGadgets(void);
  //
  // Event handling callback.
  virtual int HandleEvent(struct Event &event);
  //
public:
  // Form a warning requester. Requires only the machine class.
  FileRequester(class Machine *mach);
  ~FileRequester(void);
  //
  // Run the requester with the warning text. If this returns zero, then
  // the requester could not be build for whatever reason or the requester
  // got canceled. Returns non-zero on success. The message gets returned
  // by SelectedItem() below.
  int Request(const char *title,const char *initial,bool save = false,bool filesonly = true,bool dirsonly = false);
  //
  // After a successful request, return the selected file name.
  const char *SelectedItem(void) const
  {
    return Result;
  }
};
///

///
#endif
