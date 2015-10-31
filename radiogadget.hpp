/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: radiogadget.hpp,v 1.4 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of a mutually exclusive arrangement of buttons
 ** forming a radio-button (array)
 **********************************************************************************/

#ifndef RADIOGADGET_HPP
#define RADIOGADGET_HPP

/// Includes
#include "gadgetgroup.hpp"
#include "argparser.hpp"
///

/// Class RadioGadget
// A radio button gadget. This is a gadget group build on several
// boolean gadgets.
class RadioGadget : public GadgetGroup {
  //
  // This defines the active selection within the radio list
  LONG Selection;
  //
  // The list of items we can select from
  const struct ArgParser::SelectionVector *Items;
  //
public:
  RadioGadget(List<Gadget> &gadgetlist,
	      class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
	      const struct ArgParser::SelectionVector *items,
	      LONG initial);
  virtual ~RadioGadget(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Refresh this gadget and all gadgets inside.
  virtual void Refresh(void);
  //
  // Return the currently active selection
  LONG GetStatus(void) const
  {
    return Selection;
  }
  //
  // Set the currently active selection
  void SetStatus(LONG status);
};
///

///
#endif
