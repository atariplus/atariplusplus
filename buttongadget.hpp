/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: buttongadget.hpp,v 1.2 2005-09-10 12:55:39 thor Exp $
 **
 ** In this module: Definition of the button gadget
 **********************************************************************************/

#ifndef BUTTONGADGET_HPP
#define BUTTONGADGET_HPP

/// Includes
#include "gadget.hpp"
///

/// Class ButtonGadget
// A button type gadget that can be clicked (once) and that stays
// active as LONG as the mouse pointer is held.
class ButtonGadget : public Gadget {
protected:
  // The text we render into
  const char *ButtonText;
  //
  // This is set in case we render the gadget "as active"
  bool        HitImage;
  //
public:
  ButtonGadget(List<Gadget> &gadgetlist,
	       class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
	       const char *body);
  virtual ~ButtonGadget(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Re-render the gadget
  virtual void Refresh(void);
};
///

///
#endif
