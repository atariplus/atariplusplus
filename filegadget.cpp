/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filegadget.cpp,v 1.7 2020/07/18 15:20:39 thor Exp $
 **
 ** In this module: A string gadget with a file requester attached
 **********************************************************************************/

/// Includes
#include "filegadget.hpp"
#include "new.hpp"
///

/// FileGadget::FileGadget
// Constructor is identical to that of the StringGadget
FileGadget::FileGadget(List<Gadget> &gadgetlist,class RenderPort *rp,
		       LONG le,LONG te,LONG w,LONG h,
		       const char *initialvalue,
		       bool save,bool filesonly,bool dirsonly)
  : GadgetGroup(gadgetlist,rp,le,te,w,h),
    Text(new class StringGadget(*this,rp,le,te,w - 16,h,initialvalue)),
    Button(new class FileButtonGadget(*this,rp,le + w - 16,te,16,h)),
    ForSave(save), FilesOnly(filesonly), DirsOnly(dirsonly)
{
}
///

/// FileGadget::~FileGadget
FileGadget::~FileGadget(void)
{
  delete Text;
  delete Button;
}
///

/// FileGadget::HitTest
// Perform action if the gadget was hit, resp. release the gadget.
bool FileGadget::HitTest(struct Event &ev)
{
  //
  // First perform the hit test on the gadget group, then replace the active gadget by
  // our gadget if appropriate.
  if (GadgetGroup::HitTest(ev)) {
    switch(ev.Type) {
    case Event::GadgetDown:
    case Event::GadgetMove:
    case Event::GadgetUp:
      if (ev.Object == Text) {
	// Replace by our gadget to inform the client correctly.
	ev.Object = this;
      } else if (ev.Object == Button) {
	// Do this by a file requester filling in the result into the named
	// gadget.
	if (ev.Type == Event::GadgetUp) {
	  ev.Type      = Event::Request;	
	  ev.ControlId = (ForSave?1:0)|(FilesOnly?2:0)|(DirsOnly?4:0);
	  // Use this as an indicator whether this is a load or a save requester
	  ev.Object    = this;
	} else {
	  ev.Object    = NULL;
	}
      }
      // Fall through.
    default:
      return true;
    }
  }
  //
  return false;
}
///
