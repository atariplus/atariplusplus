/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gadget.cpp,v 1.8 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Abstract base class for all gadgets used in the GUI
 **********************************************************************************/

/// Includes
#include "gadget.hpp"
///

/// Gadget::Gadget
// Construct a gadget and add it to a list of gadgets
// A gadget per se is nothing. This class must be derived to be
// worth something.
Gadget::Gadget(List<Gadget> &gadgetlist,
		     class RenderPort *rp,LONG le,LONG te,LONG w,LONG h)
  : RPort(rp), LeftEdge(le), TopEdge(te), Width(w), Height(h), Active(false),
    UserPtr(NULL)
{
  gadgetlist.AddTail(this);
}
///

/// Gadget::~Gadget
// Remove a gadget from the gadget list, then dispose it.
Gadget::~Gadget(void)
{
  Remove();
}
///

/// Gadget::Within
// Check whether a given position lies within the gadget.
bool Gadget::Within(const struct Event &ev)
{
  if (ev.X >= LeftEdge         && ev.Y >= TopEdge &&
      ev.X <  LeftEdge + Width && ev.Y <  TopEdge + Height) {
    if (ev.X < RPort->WidthOf() && ev.Y < RPort->HeightOf()) {
      return true;
    }
  }
  return false;
}
///

/// Gadget::MoveGadget
// Adjust the position of the gadget by the indicated amount
void Gadget::MoveGadget(LONG dx,LONG dy)
{
  LeftEdge += dx;
  TopEdge  += dy;
}
///

/// Gadget::FindGadgetInDirection
// Check for the nearest gadget in the given direction dx,dy,
// return this (or a sub-gadget) if a suitable candidate has been
// found, alter x and y to a position inside the gadget, return NULL
// if this gadget is not in the direction.
const class Gadget *Gadget::FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const
{
  if ((x >  LeftEdge + Width || dx >= 0) &&
      (x <= LeftEdge         || dx <= 0) &&
      (y >  TopEdge + Height || dy >= 0) &&
      (y <= TopEdge          || dy <= 0)) {
    LONG xn = LeftEdge + (Width  >> 1);
    LONG yn = TopEdge  + (Height >> 1);
    if ((dx > 0 && xn < x) ||
	(dx < 0 && xn > x) ||
	(dy > 0 && yn < y) ||
	(dy < 0 && yn > y))
      return NULL;
    LONG xm = x - xn;
    LONG ym = y - yn;
    if (xm < 0) xm = -xm;
    if (ym < 0) ym = -ym;
    if (dx && dy == 0 && ym > xm)
      return NULL;
    if (dy && dx == 0 && xm > ym)
      return NULL;
    x = xn, y = yn;
    return this;
  }
  return NULL;
}
///

/// Gadget::FindGadgetInDirection
// Check for the nearest gadget in the given direction dx,dy,
// return this (or a sub-gadget) if a suitable candidate has been
// found, alter x and y to a position inside the gadget, return NULL
// if this gadget is not in the direction.
const class Gadget *Gadget::FindGadgetInDirection(const List<Gadget> &glist,LONG &x,LONG &y,WORD dx,WORD dy)
{
  const class Gadget *g,*gad = NULL; // The found gadget
  LONG dist = 0;    // The found distance
  LONG mx,my;       // The current x,y
  LONG xmin = x,ymin = y;   // The best candidate
  
  for(g = glist.First();g;g = g->NextOf()) {
    const class Gadget *f;
    LONG mdist;
    //
    mx = x,my = y;
    f  = g->FindGadgetInDirection(mx,my,dx,dy);
    // Is f a candidate?
    if (f) {
      // Check whether this is a better candidate than before.
      mdist = (mx - x) * (mx - x) + (my - y) * (my - y);
      if (gad == NULL || (mdist < dist)) {
	gad  = f;
	xmin = mx;
	ymin = my;
	dist = mdist;
      }
    }
  }
  //
  // Found a candidate? If so, return.
  if (gad) {
    x = xmin,y = ymin;
    return gad;
  }
  return NULL;
}
///
