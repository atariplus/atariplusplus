/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: booleangadget.hpp,v 1.4 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: Definition of a single on/off radio button
 **********************************************************************************/

#ifndef BOOLEANGADGET_HPP
#define BOOLEANGADGET_HPP

/// Includes
#include "gadget.hpp"
///

/// Class BooleanGadget
// A boolean gadget indicates a toggle-able on/off value together with
// a text explaining what it does.
class BooleanGadget : public Gadget {
  //
  const char *GadgetLabel;  // the label printed near the button
  //
  // The current selection of the boolean toggler. Can be either on
  // or off.
  bool  Toggle;
  //
public:
  BooleanGadget(List<Gadget> &gadgetlist,
		class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
		const char *label,bool initialstate);
  virtual ~BooleanGadget(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Re-render the gadget
  virtual void Refresh(void);
  //
  // Return the current toggle status.
  bool GetStatus(void) const
  {
    return Toggle;
  }
  // Set the current toggle status
  void SetStatus(bool status);
};
///

///
#endif
