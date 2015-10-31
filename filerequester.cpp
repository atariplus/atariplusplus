/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filerequester.cpp,v 1.5 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: A requester class that requests a file name from the user
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "filerequester.hpp"
#include "exceptions.hpp"
#include "textgadget.hpp"
#include "filelist.hpp"
///

/// FileRequester::FileRequester
FileRequester::FileRequester(class Machine *mach)
  : Requester(mach), FileSelections(NULL), Result(NULL)
{
}
///

/// FileRequester::~FileRequester
FileRequester::~FileRequester(void)
{
  delete[] Result;
  //
  // The gadgets are disposed by the superclass here.
  //
}
///

/// FileRequester::BuildGadgets
// Build up the gadgets for this requester. We currently
// only present a title, the contents, and an "OK" gadget.
void FileRequester::BuildGadgets(List<Gadget> &glist,class RenderPort *rport)
{
  LONG w,h;
  // Get dimension of the render port to adjust the size of the gadgets.
  w = rport->WidthOf();
  h = rport->HeightOf();  
  //
  new class TextGadget(glist,rport,0,0,w,12,Title);
  // Make a file list that fills the entire render port.
  FileSelections = new class FileList(glist,rport,0,12,w,h-12,
				      InitialFile,Saving,FilesOnly,DirsOnly);   

}
///

/// FileRequester::HandleEvent
// Event handling callback.
int FileRequester::HandleEvent(struct Event &event)
{
  if (event.Type == Event::GadgetUp && event.Object) {
    const char *result;
    // There is only one gadget that can go up, and this
    // is the file gadget
 #if CHECK_LEVEL > 0
    if (Result) {
      Throw(ObjectExists,"FileRequester::HandleEvent","the result variable exists already, forgot to clean up?");
    }
#endif
    // Is the request successful?
    if (event.Button) {
      // Extract the result from the file list
      result = FileSelections->GetStatus();
      // Copy the result into the result variable.
      Result = new char[strlen(result) + 1];
      strcpy(Result,result);
    }
    // Otherwise, the user aborted the request.
    return RQ_Abort;
  }
  return RQ_Nothing;
}
///

/// FileRequester::CleanupGadgets
// Gadget cleaner routine. Note that the
// gadgets itself are successfully destroyed by
// the superclass already.
void FileRequester::CleanupGadgets(void)
{
  // Just clean the pointers to indicate that we
  // are ready to take new gadgets here.
  FileSelections = NULL;
}
///

/// FileRequester::Request
// Build up a warning requester with the given warning message.
int FileRequester::Request(const char *title,const char *initial,bool save,bool filesonly,bool dirsonly)
{
  // Keep the initial settings for the request, then run the requester
  // main loop.
  Title       = title;
  InitialFile = initial;
  Saving      = save;
  FilesOnly   = filesonly;
  DirsOnly    = dirsonly;  
  // Clear the result variable from the last go, possibly.
  delete[] Result;
  Result = NULL;
  //
  // Go on and request now the requester.... 
  if (Requester::Request()) {
    // The requester could have been build. Now check whether
    // we got any file.
    if (Result) {
      return 1; // Yup, worked.
    }
  }
  //
  // Otherwise, signal that the user canceled the activity.
  return 0;
}
///
