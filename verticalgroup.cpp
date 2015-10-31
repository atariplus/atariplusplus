/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: verticalgroup.cpp,v 1.7 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a meta gadget keeping a vertical arrangement
 ** of gadgets that is scroll-able
 **********************************************************************************/

/// Includes
#include "verticalgroup.hpp"
#include "new.hpp"
///

/// VerticalGroup::VerticalGroup
// Create a vertical arrangements of gadget that is move-able by a slider
VerticalGroup::VerticalGroup(List<Gadget> &gadgetlist,
			     class RenderPort *rp,LONG le,LONG te,LONG w,LONG h)
  : GadgetGroup(gadgetlist,rp,le,te,w,h), 
    Slider(new class SliderGadget(SliderList,rp,le + w - 12,te,12,h,0,0x7fff,true))
{
}
///

/// VerticalGroup::~VerticalGroup
VerticalGroup::~VerticalGroup(void)
{
  class Gadget *slider;
  // Dispose all gadgets on the slider list. This should also dispose our
  // slider gadget.
  while((slider = SliderList.First()))
	delete slider;
}
///

/// VerticalGroup::Refresh
// Refresh the vertical group gadget by first refreshing the gadget group and
// then refreshing the slider.
void VerticalGroup::Refresh(void)
{
  LONG min,max;
  class Gadget *gadget;
  // Compute the topmost gadget and the extension of the gadgets in vertical
  // direction.
  min    = 0;
  max    = Height;
  for(gadget = First();gadget;gadget = gadget->NextOf()) {
    LONG gmin,gmax;
    gmin = gadget->TopEdgeOf();
    gmax = gmin + gadget->HeightOf();
    if (gmin < min)
      min = gmin;
    if (gmax > max)
      max = gmax;
  }
  AreaMin = min;
  AreaMax = max;
  // Compute the knob dimension and position.
  Slider->SetProp(Slider->GetProp(),
		  Slider->ComputeKnobSize(Height,max-min));
  GadgetGroup::Refresh();
}
///

/// VerticalGroup::ReadSlider
// Use the result of the slider gadget and adjust the position of the gadgets
// below
void VerticalGroup::ReadSlider(void)
{
  LONG top;
  class Gadget *gadget;
  //
  top = Slider->TopEntry(Slider->GetProp(),Height,AreaMax - AreaMin);
  //
  //RPort->CleanBox(LeftEdge,TopEdge,Width,Height,0x08);
  // "top" is now the first pixel to be seen, in other words, the new
  // AreaMin "negative" number.
  top = -AreaMin - top; 
  // This is the distance we have to move all gadgets within.
  if (top) {
    // If there is anything to do about it anyhow...
    for(gadget = First();gadget;gadget = gadget->NextOf()) {
      gadget->MoveGadget(0,top);
    }
    AreaMin += top;
    AreaMax += top;
    GadgetGroup::Refresh();
  }
}
///

/// VerticalGroup::ScrollTo
// Scroll to the indicated position within the vertical group
void VerticalGroup::ScrollTo(UWORD pos)
{
  Slider->SetProp(pos);  
  ReadSlider();  
}
///

/// VerticalGroup::HitTest
// Return the active gadget within the vertical group gadget. This
// never returns the slider as this is "hidden inside" of this gadget
bool VerticalGroup::HitTest(struct Event &ev)
{
  //
  // Check whether this is a wheel event. If so, we only
  // swallow it if the mouse pointer is within our gadget.
  if (ev.Type == Event::Wheel) {
    if (Within(ev)) {
      // Let the slider know that something's happening here.
      Slider->HitTest(ev);
      // Discard the event as we don't want to see the slider from
      // outside, but keep the event type to keep track of up/down events.
      ev.Object = NULL;
      //
      // Ok, the slider is active. Adjust the slider contents now.
      ReadSlider();
      //Slider->Refresh();
      return true;
    } else {
      // Ingore the event, leave it to other groups.
      return false;
    }
  }
  //
  // Check whether we are active. If not, try looking for one.
  if (ActiveGadget == NULL) {
    if (Slider->HitTest(ev)) {
      // The slider was hit. Make it active unless we have a mouse wheel event.
      ActiveGadget = Slider;
    } else {
      // Run the hittest of the gadget group. This may also result in
      // setting the active gadget.
      return GadgetGroup::HitTest(ev);
    }
  } else {
    // We have an active gadget. It might go inactive here.
    if (ActiveGadget->HitTest(ev)) {
      bool doslider = false;
      // If the active gadget is the slider, it might require updating.
      if (ActiveGadget == Slider)
	doslider = true;
      // Check whether the active gadget moved up. If so, this gadget
      // is no longer active.
      if (ev.Type == Event::GadgetUp || ev.Type == Event::Request) {
	ActiveGadget = NULL;
      }
      // Unless we're handling the slider, we're done.
      if (!doslider)
	return true;
    } else {
      // The active gadget did nothing, hence bail out.
      return false;
    }
  }
  //
  // Here: We got an event that is related to the slider that controls
  // the vertical group.
  // Discard the event as we don't want to see the slider from
  // outside, but keep the event type to keep track of up/down events.
  ev.Object = NULL;
  //
  // Ok, the slider is active. Adjust the slider contents now.
  ReadSlider();
  //Slider->Refresh();
  return true;
}
///
