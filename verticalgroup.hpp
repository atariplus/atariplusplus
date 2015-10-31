/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: verticalgroup.hpp,v 1.4 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a meta gadget keeping a vertical arrangement
 ** of gadgets that is scroll-able
 **********************************************************************************/

#ifndef VERTICALGROUP_HPP
#define VERTICALGROUP_HPP

/// Includes
#include "gadgetgroup.hpp"
#include "slidergadget.hpp"
///

/// Class VerticalGroup
// A meta-gadget that allows the gadgets inside to slide vertical
// under the control of a slider right to the gadgets
class VerticalGroup : public GadgetGroup {
  // We also have a private gadgetlist in here that contains just
  // the slider.
  List<Gadget>             SliderList;
  // Namely, this slider (must be below the list since the list must be
  // initialized first)
  class SliderGadget      *Slider;
  //
  // Dimension of the scroll-able area: topmost and last pixel
  LONG                     AreaMin,AreaMax;
  //
  // Adjust the gadget display to the setting of the slider
  void ReadSlider(void);
  // 
public:
  VerticalGroup(List<Gadget> &gadgetlist,
		class RenderPort *rp,LONG le,LONG te,LONG w,LONG h);
  virtual ~VerticalGroup(void);
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
  UWORD GetScroll(void) const
  {
    return Slider->GetProp();
  }
};
///

///
#endif
