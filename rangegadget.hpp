/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: rangegadget.hpp,v 1.5 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of a gadget describing a range with a slider and
 ** a text display
 **********************************************************************************/

#ifndef RANGEGADGET_HPP
#define RANGEGADGET_HPP

/// Includes
#include "gadgetgroup.hpp"
#include "slidergadget.hpp"
#include "textgadget.hpp"
#include "stringgadget.hpp"
///

/// Class RangeGadget
// A range gadget displays a range of numbers by a horizontal slider and
// a text gadget below.
class RangeGadget : public GadgetGroup {
  //
  // The slider gadget for this group
  class SliderGadget *Slider;
  // A text gadget for visual feedback if the number of choices is
  // small.
  class TextGadget   *Text;
  // And a string that shows the current slider stetting and
  // allows adjustment via the keyboard.
  class StringGadget *String;
  //
  // The currently active selection, and the range of selections.
  LONG Min,Max,Setting;
  //
  // A temporary buffer containing the converted string
  char Buffer[12];
  //
public:
  RangeGadget(List<Gadget> &gadgetlist,
	      class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
	      LONG min,LONG max,LONG setting);
  virtual ~RangeGadget(void);
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
    return Setting;
  }
  //
  // Set the currently active selection
  void SetStatus(LONG value);
};
///

///
#endif
