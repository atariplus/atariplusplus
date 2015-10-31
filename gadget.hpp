/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gadget.hpp,v 1.5 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Abstract base class for all gadgets used in the GUI
 **********************************************************************************/

#ifndef GADGETS_HPP
#define GADGETS_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "event.hpp"
#include "renderport.hpp"
///

/// class Gadget
// A gadget is a click-able region with the screen. This defines the
// basic user frontend object.
class Gadget : public Node<class Gadget> {
protected:
  // Where we draw into
  class RenderPort         *RPort;
  //
  // The active region of the gadget, encoded in coordinates.
  LONG LeftEdge,TopEdge;
  LONG Width,Height;
  //
  // Activation state of this gadget. If this is true, then the
  // gadget button is "down".
  bool Active;
  //
  // The thing this gadget is linked to, or NULL.
  void *UserPtr;
  //
  // A gadget per se is nothing. This class must be derived to be
  // worth something.
  Gadget(List<Gadget> &gadgetlist,
	 class RenderPort *rp,LONG le,LONG te,LONG w,LONG h);
  //
  // Check whether a given position lies within the gadget.
  bool Within(const struct Event &ev);
  //
public:
  virtual ~Gadget(void);
  // Perform action if the gadget was hit, resp. release the gadget.
  // Return true/false on whether the gadget was hit. Also return a
  // possibly modified event back.
  virtual bool HitTest(struct Event &ev) = 0;
  //
  // Re-render the gadget
  virtual void Refresh(void) = 0;
  //
  // Get some dimension information of the gadget.
  LONG WidthOf(void) const
  {
    return Width;
  }
  LONG HeightOf(void) const
  {
    return Height;
  }
  LONG LeftEdgeOf(void) const
  {
    return LeftEdge;
  }
  LONG TopEdgeOf(void) const
  {
    return TopEdge;
  }
  //
  // Adjust the position of the gadget by the indicated amount
  virtual void MoveGadget(LONG dx,LONG dy);
  //
  // Return or set the User Pointer this gadget is part of
  void *&UserPointerOf(void)
  {
    return UserPtr;
  }
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  virtual const class Gadget *FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const;
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  static const class Gadget *FindGadgetInDirection(const List<Gadget> &glist,LONG &x,LONG &y,WORD dx,WORD dy);
};
///

///
#endif
