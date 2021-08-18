/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: rangegadget.cpp,v 1.8 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Definition of a gadget describing a range with a slider and
 ** a text display
 **********************************************************************************/

/// Includes
#include "rangegadget.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "stdlib.hpp"
#include "new.hpp"
///

/// RangeGadget::RangeGadget
RangeGadget::RangeGadget(List<Gadget> &gadgetlist,
			 class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
			 LONG min,LONG max,LONG setting)
  : GadgetGroup(gadgetlist,rp,le,te,w,h), 
    // Create a slider for showing the current contents.
    Slider(new class SliderGadget(*this,RPort,le,te,w,12,
				  SliderGadget::PropPosition(setting - min,1,max-min+1),
				  SliderGadget::ComputeKnobSize(1,max-min+1),false)),
    Text(NULL), String(NULL),
    Min(min), Max(max), Setting(setting)
{
  te    += 12;
  snprintf(Buffer,12,ATARIPP_LD,setting);
  //
  // Pick a sensible representation. If there are too many choices, make it also
  // editable by a string gadget.
  if (Max - Min < 100) {
    Text   = new class TextGadget(*this,RPort,le,te,w,12,Buffer);
    Height = (Text->TopEdgeOf() + Text->HeightOf()) - TopEdge;
  } else {
    String = new class StringGadget(*this,RPort,le,te,w,12,Buffer);
    Height = (String->TopEdgeOf() + String->HeightOf()) - TopEdge;
  }
}
///

/// RangeGadget::~RangeGadget
// Dispose the range gadget. This is done within the gadget group already
RangeGadget::~RangeGadget(void)
{
}
///

/// RangeGadget::HitTest
// Run the hit test for the range gadget, i.e. update the gadget given
// the mouse status
bool RangeGadget::HitTest(struct Event &ev)
{
  // We only run the hittest for the slider as everything else
  // cannot react on the result. If the slider is "hit", we must update
  // the text gadget below.
  if (String && String->HitTest(ev)) {
    if (ev.Type == Event::GadgetUp) {
      char *endptr;
      char *buffer = NULL;
      long setting;
      //
      // A new contents was read in? Ok, check whether it is sensible.
      String->ReadContents(buffer);
      setting = strtol(buffer,&endptr,10);
      if (*endptr == 0) {
	// The entire string is valid. Ok, check whether it is within limits.
	if (setting >= Min && setting <= Max) {
	  delete[] buffer;
	  SetStatus(setting);
	  ev.Object = this;
	  return true;
	}
      }
      // Restore the old value
      delete[] buffer;
      snprintf(Buffer,12,ATARIPP_LD,Setting);
      String->SetContents(Buffer);
    }
    return true;
  } else if (Slider->HitTest(ev)) {
    Setting = Slider->TopEntry(Slider->GetProp(),1,Max - Min + 1) + Min;
    snprintf(Buffer,12,ATARIPP_LD,Setting);
    if (Text)
      Text->Refresh();
    if (String)
      String->SetContents(Buffer);
    // Return our gadget back to the caller as the builder cannot know
    // that we are a gadget group
    ev.Object = this;
    return true;
  }
  return false;
}
///

/// RangeGadget::Refresh
// Refresh the range gadget. This does almost call the refresh of
// the gadget group except that I also rebuild the setting buffer text.
void RangeGadget::Refresh(void)
{
  Setting = Slider->TopEntry(Slider->GetProp(),1,Max - Min + 1) + Min;
  snprintf(Buffer,12,ATARIPP_LD,Setting);
  GadgetGroup::Refresh();
}
///

/// RangeGadget::SetStatus
// Set the currently active selection
void RangeGadget::SetStatus(LONG value)
{
  Slider->SetProp(Slider->PropPosition(value-Min,1,Max-Min+1));
  Setting = value;
  snprintf(Buffer,12,ATARIPP_LD,value);
  if (Text)
    Text->Refresh();
  if (String)
    String->SetContents(Buffer);
}
///
