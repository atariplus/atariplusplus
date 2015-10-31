/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: requesterentry.hpp,v 1.4 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of a modified button gadget for requesters
 **********************************************************************************/

#ifndef REQUESTERENTRY_HPP
#define REQUESTERENTRY_HPP

/// Includes
#include "buttongadget.hpp"
///

/// Class RequesterEntry
// A button gadget that looks a bit different and represents an entry in the
// directory browser/file requester. It also keeps its contents itself and
// makes a copy of the string passed in.
class RequesterEntry   : public ButtonGadget {
  //
  // Entry that is represented by this gadget.
  char *Entry;
  //
  // If the following boolean is set, then this is the gadget that got picked by the user.
  bool Picked;
  //
  // The following is set for directories
  bool IsDir;
public:
  RequesterEntry(List<Gadget> &gadgetlist,class RenderPort *rp,
		 LONG le,LONG te,LONG w,LONG h,const char *body,bool isdir);
  virtual ~RequesterEntry(void);
  //
  // Return the definition of the entry
  const char *GetStatus(void) const
  {
    return Entry;
  }
  //   
  // A custom refresh method that overloads the refresh method of the button gadget
  // hence, this gadget looks different.
  virtual void Refresh(void);
  //
  // Set whether this gadget is picked or not.
  void SetPicked(bool onoff)
  {
    Picked = onoff;
    Refresh();
  }
  //
  // Get a "strcmp" like result of whether "this" is smaller than the other
  // requester entry passed in. Results in < 0 if "this" is smaller, =0 for
  // equal entries, and >0 if "this" is larger.
  int Compare(const class RequesterEntry *other);
};
///

///
#endif
