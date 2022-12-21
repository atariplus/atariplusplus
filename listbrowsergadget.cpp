/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: listbrowsergadget.cpp,v 1.11 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: A gadget showing a list of text gadgets
 **********************************************************************************/

/// Includes
#include "listbrowsergadget.hpp"
#include "renderport.hpp"
#include "verticalgroup.hpp"
#include "separatorgadget.hpp"
#include "new.hpp"
#include "string.hpp"
#include <ctype.h>
///

/// ListBrowserGadget::TextKeeperGadget::TextKeeperGadget
// Build up the text keeper: This copies "s" characters of the
// body and manages the memory for this gadget.
ListBrowserGadget::TextKeeperGadget::TextKeeperGadget(List<Gadget> &gadgetlist,
						      class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
						      const char *body,size_t s)
  : TextGadget(gadgetlist,rp,le,te,w,h,NULL), Text(new char[s+1])
{
  //
  memcpy(Text,body,s);
  Text[s]    = '\0';
  GadgetText = Text;
}
///

/// ListBrowserGadget::TextKeeperGadget::~TextKeeperGadget
ListBrowserGadget::TextKeeperGadget::~TextKeeperGadget(void)
{
  // Dispose the text as part of our job.
  delete[] Text;
}
///

/// ListBrowserGadget::TextKeeperGadget::Refresh
// Rebuild the textkeeper gadget. Similar to the text gadget,
// but left-aligned.
void ListBrowserGadget::TextKeeperGadget::Refresh(void)
{
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,0x08);
  RPort->TextClipLefty(LeftEdge+2,TopEdge+2,Width-2,Height-4,GadgetText,15);
}
///

/// ListBrowserGadget::ListBrowserGadget
ListBrowserGadget::ListBrowserGadget(List<Gadget> &glist,class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
				     List<TextNode> *contents)
  : Gadget(glist,rp,le,te,w,h),
    ClipRegion(new class RenderPort(rp,le+1,te+1,w,h-2)), 
    Vertical(new class VerticalGroup(SubGadgets,ClipRegion,0,0,w - 2,h - 2))
{
  // Now build-up all sub-gadgets. Problem is that if we fail here, then our
  // gadget counts as non-constructed and we have to dispose the sub-gadgets
  // ourselfs.
  try {
    int y = 0;
    size_t cwidth;
    struct TextNode *node;
    bool buildseparator = false;
    bool addedtext      = false;
    //
    // Compute the width of the list in characters.
    cwidth = (w - 26) >> 3;
    //
    // Build up gadgets in the list one after another.
    for(node = contents->First();node;node = node->NextOf()) {
      const char *text;
      //
      // Get the text we want to display. Clip it into the available range.
      text = node->Text();
      while(text && *text) {
	const char *ct;
	size_t len;
	// Advance over a single \n from the last line here.
	if (*text == '\n')
	  text++;
	// Advance over white-spaces here.
	while(*text && *text!='\n' && isspace(*text)) {
	  text++;
	}
	// If we are now at the end of the text, bail out.
	if (*text == '\0')
	  break;
	//
	// Get the size of the remaining text. At most, up to the next \n
	ct = text;
	while(*ct && *ct!='\n')
	  ct++;
	len = ct - text;
	// Check whether we must clip the text. This happens only if the length is too large
	if (len > cwidth) {
	  const char *end = text + cwidth; // last sensible character where we could cut.
	  // Does not fit. Hence, break the string. If so, then try to find a white space
	  // between the current position and the end.
	  while(end > text) {
	    if (isspace(*end))
	      break;
	    end--;
	  }
	  // If we are sitting now at a white-space, go back to the next non-whitespace
	  while(end > text && isspace(*end)) {
	    end--;
	  }
	  // Now check whether we found a white space before we run out of characters.
	  // If so, then break there.
	  if (end > text) {
	    // end points now directly at the last non-white space. Hence, the
	    // size is the end-text.
	    len = end - text + 1;
	  } else {
	    // Otherwise, no sensible white-space is available. Just hard-break
	    // at the position.
	    len = cwidth;
	  }
	}
	// Check whether we have a left-over separator gadget from the last the
	// last go. If so, then insert a separator gadget here.
	if (buildseparator) {
	  new SeparatorGadget(*Vertical,ClipRegion,2,y,w - 24,12);
	  y += 12;
	  buildseparator = false;
	}
	// Build a new gadget at this position, keep the text, add it to the vertical
	// group.
	new class TextKeeperGadget(*Vertical,ClipRegion,2,y,w - 24,12,text,len);
	addedtext      = true;
	y += 12;
	//
	// Advance the text by the size.
	text += len;
      }
      if (addedtext) {
	buildseparator = true;
	addedtext      = false;
      }
      // End of text found, go to the next node.
    }
    //
    // Let the vertical group refresh and recalculate the layout.
    Vertical->Refresh();
  } catch(...) {
    // Failed construction. Dispose all the contents we allocated so far.
    // This disposes also the subgadgets.
    delete Vertical;
    // Delete the clipregion as well.
    delete ClipRegion;
    throw;
  }
}
///

/// ListBrowserGadget::~ListBrowserGadget
// Dispose the list browser and its contents
ListBrowserGadget::~ListBrowserGadget(void)
{
  delete Vertical;
  delete ClipRegion;
}
///

/// ListBrowserGadget::HitTest
// Perform the hit-test on the list browser gadget: 
// This is done by the vertical group.
bool ListBrowserGadget::HitTest(struct Event &ev)
{
  bool result;
  // Update the event X,Y to the coordinates of the clipped inner gadget
  ev.X  -= LeftEdge + 1;
  ev.Y  -= TopEdge  + 1;
  result = Vertical->HitTest(ev);
  // Restore the coordinates.
  ev.X  += LeftEdge + 1;
  ev.Y  += TopEdge  + 1;
  return result;
}
///

/// ListBrowserGadget::Refresh
// Perform the refresh of this gadget. This also draws a frame
// around the list.
void ListBrowserGadget::Refresh(void)
{
  Vertical->Refresh();
  // Now draw a frame around the text.
  RPort->Draw3DFrame(LeftEdge,TopEdge,Width - 18,Height,true);
}
///

/// ListBrowserGadget::ScrollTo
// Scroll to the indicated position: Leave this to the vertical group
void ListBrowserGadget::ScrollTo(UWORD position)
{
  Vertical->ScrollTo(position);
}
///

/// ListBrowserGadget::GetScroll
UWORD ListBrowserGadget::GetScroll(void) const
{
  return Vertical->GetScroll();
}
///

/// ListBrowserGadget::MoveGadget
// Move the gadget by the indicated position.
void ListBrowserGadget::MoveGadget(LONG dx,LONG dy)
{
  // The vertical group moves itself its subgadgets
  Vertical->MoveGadget(dx,dy);
}
///
