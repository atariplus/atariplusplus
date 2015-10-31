/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filegadget.hpp,v 1.5 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: A string gadget with a file requester attached
 **********************************************************************************/

#ifndef FILEGADGET_HPP
#define FILEGADGET_HPP

/// Includes
#include "gadgetgroup.hpp"
#include "stringgadget.hpp"
#include "filebuttongadget.hpp"
///

/// Class FileGadget
// A meta gadget consisting of a string gadget and a "select file" button right to
// it.
class FileGadget : public GadgetGroup {    
  //
  // The string gadget within here.
  class StringGadget     *Text;
  //
  // The button right to it.
  class FileButtonGadget *Button;
  //
  // Boolean whether this is for loading or for saving
  bool  ForSave;
  bool  FilesOnly;
  bool  DirsOnly;
  //
public:
  // Constructor is identical to that of the StringGadget
  FileGadget(List<Gadget> &gadgetlist,class RenderPort *rp,
	     LONG le,LONG te,LONG w,LONG h,
	     const char *initialvalue,bool save,bool filesonly,bool dirsonly);
  ~FileGadget(void);
  //
  // Return the current setting of the string gadget without allocating a new
  // buffer for it.
  const char *GetStatus(void) const
  {
    return Text->GetStatus();
  }
  //    
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Read the contents of this gadget, allocate a new buffer and fill
  // the result in.
  void ReadContents(char *&var)
  {
    Text->ReadContents(var);
  }
  //
  // Set the contents of this gadget.
  void SetContents(const char *var)
  {
    Text->SetContents(var);
  }
};
///

///
#endif
