/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: listbrowsergadget.hpp,v 1.6 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: A gadget showing a list of text gadgets
 **********************************************************************************/

#ifndef LISTBROWSERGADGET_HPP
#define LISTBROWSERGADGET_HPP

/// Includes
#include "list.hpp"
#include "gadget.hpp"
#include "textgadget.hpp"
#include "string.hpp"
///

/// Forwards
class VerticalGroup;
class RenderPort;
///

/// Class TextNode
// A node that can deliver text: This is the abstract
// base for all list nodes we can display here.
struct TextNode : public Node<struct TextNode> {
  //
public:
  virtual ~TextNode(void)
  { }
  virtual const char *Text(void) const = 0;
};
///

/// Class ListBrowserGadget
// This gadget represents a browse-able list of text containing
// gadgets. It is used here to display the warning logs.
class ListBrowserGadget : public Gadget {
  //
  // The region we clip into.
  class RenderPort       *ClipRegion;
  //
  // The list containing all gadgets collected directly
  // in this class. This is only the vertical group below.
  List<class Gadget>      SubGadgets;
  //
  // The vertical group containing the gadgets. We do not derive
  // directly from it since we render into a clipped area here.
  class VerticalGroup    *Vertical;
  //
  // A private descendent of the text gadget.
  class TextKeeperGadget : public TextGadget {
    //
    // The text we need to keep.
    char *Text;
    //
  public:
    // Build up the text keeper: This copies "s" characters of the
    // body and manages the memory for this gadget.
    TextKeeperGadget(List<class Gadget> &gadgetlist,
		     class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
		     const char *body,size_t s);
    ~TextKeeperGadget(void);  
    //
    // Refresh the contents
    virtual void Refresh(void);
  };
  //
public:
  ListBrowserGadget(List<Gadget> &glist,class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
		    List<TextNode> *contents);
  ~ListBrowserGadget(void);
  //
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Refresh this gadget and all gadgets inside.
  virtual void Refresh(void);
  //
  // Scroll to the indicated position in the area
  void ScrollTo(UWORD position);
  //
  // Get the scrolled position
  UWORD GetScroll(void) const;  
  //
  // Adjust the position of the gadget by the indicated amount
  virtual void MoveGadget(LONG dx,LONG dy);
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  virtual const class Gadget *FindGadgetInDirection(LONG &,LONG &,WORD,WORD) const
  {
    return NULL;
  }
};
///

///
#endif
