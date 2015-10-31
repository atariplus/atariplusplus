/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: textgadget.cpp,v 1.4 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a text display gadget
 **********************************************************************************/

/// Includes
#include "textgadget.hpp"
///

/// TextGadget::TextGadget
// This gadget does not react on user input. Instead, it just prints a text.
TextGadget::TextGadget(List<Gadget> &gadgetlist,
		       class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
		       const char *body)
  : Gadget(gadgetlist,rp,le,te,w,h), GadgetText(body)
{ }
///

/// TextGadget::~TextGadget
TextGadget::~TextGadget(void)
{
}
///

/// TextGadget::HitTest
// Test whether this gadget is hit by the mouse. The answer is always no as
// this gadget does not allow any user interaction
bool TextGadget::HitTest(struct Event &)
{
  return false;
}
///

/// TextGadget::Refresh
// Refresh the Text Gadget by re-rendering the text
void TextGadget::Refresh(void)
{
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,0x08);
  RPort->TextClip(LeftEdge+2,TopEdge+2,Width-4,Height-4,GadgetText,15);
}
///
