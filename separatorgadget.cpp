/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: separatorgadget.cpp,v 1.4 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of a separator bar gadget
 **********************************************************************************/

/// Includes
#include "separatorgadget.hpp"
///

/// SeparatorGadget::SeparatorGadget
// The SeparatorGadget draws a horizontal separation bar. It does not
// react on user input either.
SeparatorGadget::SeparatorGadget(List<Gadget> &gadgetlist,
				 class RenderPort *rp,
				 LONG le,LONG te,LONG w,LONG h)
  : Gadget(gadgetlist,rp,le,te,w,h)
{ }
///

/// SeparatorGadget::~SeparatorGadget
SeparatorGadget::~SeparatorGadget(void)
{
}
///

/// SeparatorGadget::HitTest
// Test whether this gadget is hit by the mouse. The answer is always no as
// this gadget does not allow any user interaction
bool SeparatorGadget::HitTest(struct Event &)
{
  return false;
}
///

/// SeparatorGadget::Refresh
// Refresh the Separator Gadget by re-rendering the text
void SeparatorGadget::Refresh(void)
{
  LONG te = TopEdge + ((Height - 2) >> 1);
  
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,0x08);
  //RPort->CleanBox(LeftEdge,te+1,Width,2,0x00);
  RPort->Draw3DFrame(LeftEdge,te,Width,2,true,0x0c,0x06);
}
///
