/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: textgadget.hpp,v 1.5 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a text display gadget
 **********************************************************************************/

#ifndef TEXTGADGET_HPP
#define TEXTGADGET_HPP

/// Includes
#include "gadget.hpp"
///

/// Class TextGadget
// The TextGadget does not react on user input. Instead, it just prints a text.
class TextGadget  : public Gadget {
protected:
  //
  // The text to print to
  const char *GadgetText;
  //
public:
  TextGadget(List<Gadget> &gadgetlist,
	     class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
	     const char *body);
  virtual ~TextGadget(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Re-render the gadget
  virtual void Refresh(void); 
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
