/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: slidergadget.hpp,v 1.4 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a slider gadget representing a range.
 **********************************************************************************/

#ifndef SLIDERGADGET_HPP
#define SLIDERGADGET_HPP

/// Includes
#include "gadget.hpp"
///

/// Class SliderGadget
// A slider gadget that can be used to represent a value within a
// range of values.
class SliderGadget : public Gadget {
  // Set if the user is currently dragging the knob.
  bool  Dragging;
  // The current position. This is a fix-point number in the range
  // 0x0000..0xffff
  UWORD Position;
  // The size of the gadget knob as a fixed-point number
  UWORD KnobSize;
  //
  // The absolute position of the knob
  LONG KnobLeftEdge,KnobTopEdge,KnobWidth,KnobHeight;
  //
  // A boolean that is set if the slider is a vertical slider. Otherwise,
  // it is a horizontal slider.
  bool  FreeVert;
  //
  // Compute the absolute position and size of the knob from the relative
  // coordinates
  void ToAbsolute(void);
  // Convert the absolute position back to a relative position
  void ToRelative(void);
  //
  // Adjust the knob to follow the mouse position.
  void FollowMouse(LONG x,LONG y);
public:    
  SliderGadget(List<Gadget> &gadgetlist,
	       class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
	       UWORD position,UWORD knob,bool freevert);
  virtual ~SliderGadget(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Re-render the gadget
  virtual void Refresh(void);
  //
  // Read the position of the gadget given the 
  UWORD GetProp(void)
  {
    return Position;
  }
  // Set the gadget knob position
  void SetProp(UWORD position);
  void SetProp(UWORD position,UWORD knob);
  //
  // Implement a custom MoveGadget that also moves the knob.
  virtual void MoveGadget(LONG dx,LONG dy)
  {
    LeftEdge     += dx;
    TopEdge      += dy;
    KnobLeftEdge += dx;
    KnobTopEdge  += dy;
  }
  //
  // Convert the relative prop position into an index given that
  // we see "visible" items of a list of "total" entries.
  static LONG TopEntry(UWORD prop,LONG visible,LONG total);
  // Compute the knob size for a list with "visible" items of
  // "total" entries.
  static UWORD ComputeKnobSize(LONG visible,LONG total);
  // Compute the slider position for a list with "visible" items of
  // "total" entries if the topmost entry is "top".
  static UWORD PropPosition(LONG top,LONG visible,LONG total);
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  const class Gadget *FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const;
};
///

///
#endif
