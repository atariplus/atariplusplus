/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: slidergadget.cpp,v 1.9 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a slider gadget representing a range.
 **********************************************************************************/

/// Includes
#include "slidergadget.hpp"
///

/// SliderGadget::SliderGadget
// Construct a new slider
SliderGadget::SliderGadget(List<Gadget> &gadgetlist,
				 class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
				 UWORD position,UWORD knob,bool freevert)
  : Gadget(gadgetlist,rp,le,te,w,h),
    Dragging(false), Position(position), KnobSize(knob), FreeVert(freevert)
{ 
  ToAbsolute();
}
///

/// SliderGadget::~SliderGadget
SliderGadget::~SliderGadget(void)
{
}
///

/// SliderGadget::ToAbsolute
// Compute the absolute position and size of the knob from the relative
// coordinates
void SliderGadget::ToAbsolute(void)
{
  LONG absposition,abssize;

  if (FreeVert) {
    abssize      = ((Height - 4) * KnobSize + 0x7fff) / 0xffff; 
    if (abssize < 14) abssize = 14;
    absposition  = TopEdge  + 2 + ((Height - 4 - abssize) * Position + 0x7fff) / 0xffff;
    KnobLeftEdge = LeftEdge + 2;
    KnobTopEdge  = absposition;
    KnobWidth    = Width - 4;
    KnobHeight   = abssize;
  } else {
    abssize      = ((Width  - 4) * KnobSize + 0x7fff) / 0xffff;  
    if (abssize < 14) abssize = 14;
    absposition  = LeftEdge + 2 + ((Width  - 4 - abssize) * Position + 0x7fff) / 0xffff;
    KnobLeftEdge = absposition;
    KnobTopEdge  = TopEdge + 2;
    KnobWidth    = abssize;
    KnobHeight   = Height - 4;
  }
}
///

/// SliderGadget::ToRelative
// Convert the absolute knob position back to a relative
// fixpoint number
void SliderGadget::ToRelative(void)
{
  LONG freedom;
  
  if (FreeVert) {
    freedom   = Height - 4 - KnobHeight;
    if (freedom > 0) 
      Position  = UWORD(((KnobTopEdge  - TopEdge  - 2) * 0xffff + (freedom >> 1)) / freedom);
    else
      Position  = 0;
  } else {
    freedom   = Width  - 4 - KnobWidth;
    if (freedom > 0) 
      Position  = UWORD(((KnobLeftEdge - LeftEdge - 2) * 0xffff + (freedom >> 1)) / freedom);
    else
      Position  = 0;
  }
}
///

/// SliderGadget::FollowMouse
// Adjust the slider knob such that it is under the mouse
void SliderGadget::FollowMouse(LONG x,LONG y)
{
  if (FreeVert) {
    KnobTopEdge = y - (KnobHeight >> 1);
    if (KnobTopEdge < TopEdge + 2) {
      KnobTopEdge = TopEdge + 2;
    }
    if (KnobTopEdge + KnobHeight > TopEdge + Height - 2) {
      KnobTopEdge = TopEdge + Height - 2 - KnobHeight;
    }
  } else {
    KnobLeftEdge = x - (KnobWidth >> 1);
    if (KnobLeftEdge < LeftEdge + 2) {
      KnobLeftEdge = LeftEdge + 2;
    }
    if (KnobLeftEdge + KnobWidth > LeftEdge + Width - 2) {
      KnobLeftEdge = LeftEdge + Width - 2 - KnobWidth;
    }
  }
}
///

/// SliderGadget::HitTest
bool SliderGadget::HitTest(struct Event &ev)
{
  switch(ev.Type) {
  case Event::Wheel:
    // Mouse scroll wheel. Works only if we are active and not dragging.
    if (Dragging == false && FreeVert) {
      LONG move;
      LONG newpos;
      // Update the position 
      move   = (KnobSize * ev.ScrolledLines) >> 2;
      if (move == 0)
	move = ev.ScrolledLines;
      newpos = Position + move;
      if (newpos < 0) {
	newpos = 0;
      } else if (newpos > 0xffff) {
	newpos = 0xffff;
      }
      Position = UWORD(newpos);
      ToAbsolute();
      Refresh();
      ev.Type   = Event::GadgetMove;
      ev.Object = this;
      return true;
    }
    // Ignore this event.
    return false;
  case Event::Mouse:
    // Mouse movement. Follow the mouse if we are "dragging". Otherwise,
    // ignore it.
    if (Active && ev.Button) {
      // If we are dragging, update the slider position. Otherwise, do nothing
      // as the user just clicked into the container.
      if (Dragging) {
	LONG kle,kte,kw,kh;
	// Keep old coordinates for efficient update
	kle = KnobLeftEdge;
	kte = KnobTopEdge;
	kw  = KnobWidth;
	kh  = KnobHeight;
	FollowMouse(ev.X,ev.Y);
	ToRelative();
	if (kle != KnobLeftEdge || kte != KnobTopEdge ||
	    kw  != KnobWidth    || kh  != KnobHeight) {
	  Refresh();
	}
	ev.Object = this;
      } else {
	ev.Object = NULL;
      }
      ev.Type = Event::GadgetMove;
      return true;
    }
    return false;
  case Event::Click:
    // Is not active. If we are within the gadget, activate us
    if (ev.Button && Within(ev)) {
      // Is within the gadget, activate us. 
      Active = true;
      // Are we also within the knob?
      ToAbsolute();
      if (ev.X >= KnobLeftEdge && ev.X < KnobLeftEdge + KnobWidth  &&
	  ev.Y >= KnobTopEdge  && ev.Y < KnobTopEdge  + KnobHeight) {
	// If so, enable dragging
	Dragging = true;
	// Move the knob onto the mouse.
	ToAbsolute();
	FollowMouse(ev.X,ev.Y);
	Refresh();
      } else {
	bool decrement = false;
	Dragging = false;
	// Outside of the knob. Jump to the corresponding side and
	// re-render the gadget
	if (FreeVert) {
	  if (ev.Y < KnobTopEdge)
	    decrement = true;
	} else {
	  if (ev.X < KnobLeftEdge)
	    decrement = true;
	}
	// Jump by one knobsize into the indicated direction.
	if (decrement) {
	  if (Position > KnobSize) {
	    Position -= KnobSize;
	  } else {
	    Position  = 0;
	  }
	} else {
	  if (Position < 0xffff-KnobSize) {
	    Position += KnobSize;
	  } else {
	    Position  = 0xffff;
	  }
	}
	ToAbsolute();
	Refresh();
      }
      ev.Type   = Event::GadgetDown;
      ev.Object = this;
      return true;
    } else if (ev.Button == false && Active) {
      // The button goes up and we are active. Go inactive and return a
      // gadgetup
      ev.Type   = Event::GadgetUp;
      ev.Object = this;
      Active    = false;
      Dragging  = false;
      return true;
    }
  default:
    return false;
  }
}
///

/// SliderGadget::Refresh
// Re-draw the slider gadget
void SliderGadget::Refresh(void)
{
  LONG le,te,w,h,i;
  // Fill in dark color
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,0x08);
  // Render a frame around it.
  if (FreeVert) {
    RPort->Draw3DFrame(LeftEdge + (Width >> 1)-1,TopEdge,2,Height,false,0x02,0x0a);
  } else {
    RPort->Draw3DFrame(LeftEdge,TopEdge + (Height >> 1) - 1,Width,2,false,0x02,0x0a);
  }    
  // Render the knob.
  // The absolute position of the knob
  RPort->CleanBox(KnobLeftEdge,KnobTopEdge,KnobWidth,KnobHeight,0x0a);
  // Render a frame around it
  RPort->Draw3DFrame(KnobLeftEdge,KnobTopEdge,KnobWidth,KnobHeight,false,0x0e,0x06);
  // Render the knob handle in the middle of the knob.
  if (FreeVert) {
    le = KnobLeftEdge + 2;
    te = KnobTopEdge  + ((KnobHeight - 10 - 4) >> 1) + 2;
    w  = KnobWidth    - 4;
    for(i=0;i<10;i++) {
      RPort->SetPen((UBYTE)((i & 1)?(0x0c):(0x08)));
      RPort->Position(le,te);
      RPort->DrawHorizontal(w);
      te++;
    }
  } else {
    le = KnobLeftEdge + ((KnobWidth  - 10 - 4) >> 1) + 2;
    te = KnobTopEdge  + 2;
    h  = KnobHeight   - 4;   
    for(i=0;i<10;i++) {
      RPort->SetPen((i & 1)?(0x0c):(0x08));
      RPort->Position(le,te);
      RPort->DrawVertical(h);
      le++;
    }
  }
}
///

/// SliderGadget::SetProp
// Set the position of the slider (without changing the size of the knob)
void SliderGadget::SetProp(UWORD position)
{
  if (!Active) {
    // Do not allow modification if the user is just playing with the gadget
    Position = position;
    ToAbsolute();
  }
  Refresh();
}
///

/// SliderGadget::SetProp
// Set the position of the slider and the size of the knob
void SliderGadget::SetProp(UWORD position,UWORD knob)
{
  if (!Active) {
    Position = position;
    KnobSize = knob;
    ToAbsolute();
  }
  Refresh();
}
///

/// SliderGadget::TopEntry
// Convert the relative prop position into an index given that
// we see "visible" items of a list of "total" entries.
LONG SliderGadget::TopEntry(UWORD prop,LONG visible,LONG total)
{
  LONG top;
  
  if (visible >= total) {
    return 0; 
    // As we can always see all entries, there is no need to
    // shift off some entries
  } else {
    top = (LONG)((((QUAD)prop) * (total - visible) + 0x7fffL) / 0xffffL);
    if (top > total - visible)
      return  total - visible;
    return top;
  }
}
///

/// SliderGadget::ComputeKnobSize
// Compute the knob size for a list with "visible" items of
// "total" entries.
UWORD SliderGadget::ComputeKnobSize(LONG visible,LONG total)
{
  LONG knobsize;
  
  if (visible >= total) {
    return 0xffff; // There is no need for a knob
  } else {
    // The knob size displays the amount of entries visible relative
    // to the amount of total entries.
    knobsize = (visible * 0xffff + (total >> 1)) / total;
    if (knobsize > 0xffff)
      return 0xffff;
    return UWORD(knobsize);
  }
}
///

/// SliderGadget::PropPosition
// Compute the slider position for a list with "visible" items of
// "total" entries if the topmost entry is "top".
UWORD SliderGadget::PropPosition(LONG top,LONG visible,LONG total)
{
  LONG sliderpos,selections;
  
  if (visible >= total) {
    // All items are visible. Hence display the slider on top of the list.
    return 0;
  } else {
    // The slider presents the selection of one of the total - visible entries
    // within the total list.
    selections = total - visible;
    sliderpos  = (((ULONG)top) * 0xffffUL + (ULONG)(selections >> 1)) / selections;
    if (sliderpos > 0xffff)
      return 0xffff;
    return UWORD(sliderpos);
  }
}
///

/// SliderGadget::FindGadgetInDirection
// Check for the nearest gadget in the given direction dx,dy,
// return this (or a sub-gadget) if a suitable candidate has been
// found, alter x and y to a position inside the gadget, return NULL
// if this gadget is not in the direction.
#include<stdio.h>
const class Gadget *SliderGadget::FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const
{
  if (FreeVert) {
    if ((x >  LeftEdge + Width || dx >= 0) &&
	(x <= LeftEdge         || dx <= 0)) {
      LONG xn = x,yn = y;
      LONG ym = TopEdge + (Height  >> 1);
      //
      if (x >= LeftEdge && x < LeftEdge + Width) {
	if (dy < 0) {
	  yn = (y <= ym)?(TopEdge):(ym); 
	  if (yn > y) return NULL;
	} else if (dy > 0) {
	  yn = (y >= ym)?(TopEdge + Height - 1):(ym);
	  if (yn < y) return NULL;
	} 
      } else if (dx != 0) { 
	yn = ym;
      } else if ((dy > 0 && ym > y) ||
		 (dy < 0 && ym < y)) {
	LONG xc = x - (LeftEdge + (Width >> 1));
 	LONG yc = y - ym;
	if (xc < 0) xc = -xc;
	if (yc < 0) yc = -yc;
	if (yc > xc) {
	  yn = ym;
	} else return NULL;
      } else return NULL;
      xn = LeftEdge  + (Width >> 1);
      if (xn != x || yn != y) {
	x = xn, y = yn;
	return this;
      } else if (dx == 0 && dy == 0) {
	return this;
      }
    }
  } else {
    if ((y >  TopEdge + Height || dy >= 0) &&
	(y <= TopEdge          || dy <= 0)) {
      LONG xn = x,yn = y;
      LONG xm = LeftEdge + (Width  >> 1);
      //
      if (y >= TopEdge && y < TopEdge + Height) {
	if (dx < 0) {
	  xn = (x <= xm)?(LeftEdge):(xm);
	  if (xn > x) return NULL;
	} else if (dx > 0) {
	  xn = (x >= xm)?(LeftEdge + Width - 1):(xm);
	  if (xn < x) return NULL;
	} 
      } else if (dy != 0) { 
	xn = xm;
      } else if ((dx > 0 && xm > x) ||
		 (dx < 0 && xm < x)) {
 	LONG xc = x - xm;
	LONG yc = y - (TopEdge + (Height >> 1));
	if (xc < 0) xc = -xc;
	if (yc < 0) yc = -yc;
	if (xc > yc) {
	  xn = xm;
	} else return NULL;
      } else return NULL;
      yn = TopEdge  + (Height >> 1);
      if (xn != x || yn != y) {
	x = xn, y = yn;
	return this;
      } else if (dx == 0 && dy == 0) {
	return this;
      }
    }
  }
  return NULL;
}
///
