/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: requesterentry.cpp,v 1.5 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of a modified button gadget for requesters
 **********************************************************************************/

/// Includes
#include "requesterentry.hpp"
#include "new.hpp"
#include "string.hpp"
///

/// RequesterEntry::RequesterEntry
// Construct a gadget representing one entry of a requester/list
RequesterEntry::RequesterEntry(List<Gadget> &gadgetlist,
				     class RenderPort *rp,
				     LONG le,LONG te,LONG w,LONG h,
				     const char *body,bool isdir)
  : ButtonGadget(gadgetlist,rp,le,te,w,h,NULL), Entry(body?(new char[strlen(body)+2]):NULL), 
    Picked(false), IsDir(isdir)
{
  if (body) {
    strcpy(Entry,body);
    if (isdir) {
      // Attach a "/" to indicate that we are a directory.
      strcat(Entry,"/");
    }
    // We must update the entry pointer here as it did not exist before.
    ButtonText = Entry;
  }
}
///

/// RequesterEntry::~RequesterEntry
RequesterEntry::~RequesterEntry(void)
{
  delete[] Entry;
}
///

/// RequesterEntry::Compare
// Get a "strcmp" like result of whether "this" is smaller than the other
// requester entry passed in. Results in < 0 if "this" is smaller, =0 for
// equal entries, and >0 if "this" is larger.
int RequesterEntry::Compare(const class RequesterEntry *other)
{
  // If this is a directory, but the other not, then we are smaller.
  if (IsDir == true && other->IsDir == false) {
    return -1;
  }
  if (IsDir == false && other->IsDir == true) {
    return 1;
  }
  return strcasecmp(Entry,other->Entry);
}
///

/// RequesterEntry::Refresh
// Refresh the button gadget frame and text. This replaces the buttongadget
// refresh method.
void RequesterEntry::Refresh(void)
{
  UBYTE frontpen,backpen;

  backpen  = 15;
  frontpen = 0;

  if (ButtonText) {
    if (HitImage) {
      backpen  = 0;
      frontpen = 15;
    } else {
      if (Picked) {
	backpen  = 4;
	frontpen = 15;
      }
    }
  }
  RPort->CleanBox(LeftEdge,TopEdge,1,Height,2);
  RPort->CleanBox(LeftEdge + Width - 2,TopEdge,1,Height,2);
  RPort->CleanBox(LeftEdge+1,TopEdge,Width-3,Height,backpen);
  if (ButtonText)
    RPort->TextClipLefty(LeftEdge+4,TopEdge,Width-8,Height,ButtonText,frontpen);
}
///
