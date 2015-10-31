/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: errorrequester.cpp,v 1.12 2014/06/01 20:07:53 thor Exp $
 **
 ** In this module: A requester class that prints and logs errors.
 **********************************************************************************/

/// Includes
#include "errorrequester.hpp"
#include "buttongadget.hpp"
#include "textgadget.hpp"
#include "listbrowsergadget.hpp"
#include "machine.hpp"
#include "display.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "new.hpp"
#include <stdarg.h>
///

/// ErrorRequester::ErrorRequester
ErrorRequester::ErrorRequester(class Machine *mach)
  : Requester(mach), Headline(NULL), OKGadget(NULL), 
    machine(mach), active(false)
{
}
///

/// ErrorRequester::~ErrorRequester
ErrorRequester::~ErrorRequester(void)
{
  struct TextNode *warn;
  // 
  // Dispose all warnings here. These have virtual
  // destructors, so everything gets deleted right here.
  while ((warn = ErrorLog.RemHead())) {
    delete warn;
  }
  //
  // The gadgets are disposed by the superclass here.
}
///

/// ErrorRequester::ErrorTxt::ErrorTxt
// Constructor of the error text requester
ErrorRequester::ErrorTxt::ErrorTxt(void)
  : Error(NULL)
{ 
}
///

/// ErrorRequester::ErrorTxt::~ErrorTxt
// Dispose the error text
ErrorRequester::ErrorTxt::~ErrorTxt(void)
{
  delete[] Error;
}
///

/// ErrorRequester::BuildGadgets
// Build up the gadgets for this requester. We currently
// only present a title, the contents, and an "Retry" and
// "Cancel" gadget.
void ErrorRequester::BuildGadgets(List<Gadget> &glist,class RenderPort *rport)
{
  class ListBrowserGadget *lb;
  LONG w,h;
  LONG gw;
  // Just some consistency checks.
#if CHECK_LEVEL > 0
  if (Headline || OKGadget || CancelGadget)
    Throw(ObjectExists,"ErrorRequester::BuildGadgets","requester is already build up");
#endif
  // Get dimension of the render port to adjust the size of the gadgets.
  w = rport->WidthOf();
  h = rport->HeightOf();
  //
  Headline     = new TextGadget(glist,rport,0,0,w,12,"Atari++ Fault");
  //
  lb           = new ListBrowserGadget(glist,rport,4,18,w - 8,h - 18 - 18,&ErrorLog);
  // Scroll the scroller to the bottommost position to ensure that we see the
  // last message which is at the bottom.
  lb->ScrollTo(0xffff);
  //
  // Add up additional gadgets.
#ifdef BUILD_MONITOR
  gw = w >> 2;
  CancelGadget  = new ButtonGadget(glist,rport,0   ,h - 12,gw,12,"Cancel");
  MenuGadget    = new ButtonGadget(glist,rport,gw  ,h - 12,gw,12,"Menu");
  MonitorGadget = new ButtonGadget(glist,rport,gw*2,h - 12,gw,12,"Monitor");
  OKGadget      = new ButtonGadget(glist,rport,gw*3,h - 12,gw,12,"Retry");
#else  
  gw = w / 3;
  CancelGadget  = new ButtonGadget(glist,rport,0   ,h - 12,gw,12,"Cancel");
  MenuGadget    = new ButtonGadget(glist,rport,gw  ,h - 12,gw,12,"Menu");
  MonitorGadget = NULL;
  OKGadget      = new ButtonGadget(glist,rport,gw*2,h - 12,gw,12,"Retry");
#endif
}
///

/// ErrorRequester::HandleEvent
// Event handling callback.
int ErrorRequester::HandleEvent(struct Event &event)
{
  if (event.Type == Event::GadgetUp) {
    if (event.Object == OKGadget)
      return ERQ_Retry;
    if (event.Object == CancelGadget)
      return ERQ_Cancel;
    if (event.Object == MenuGadget)
      return ERQ_Menu;
    if (event.Object == MonitorGadget)
      return ERQ_Monitor;
  }
  return ERQ_Nothing;
}
///

/// ErrorRequester::CleanupGadgets
// Gadget cleaner routine. Note that the
// gadgets itself are successfully destroyed by
// the superclass already.
void ErrorRequester::CleanupGadgets(void)
{
  // Just clean the pointers to indicate that we
  // are ready to take new gadgets here.
  Headline     = NULL;
  OKGadget     = NULL;
  MenuGadget   = NULL;
  CancelGadget = NULL;
}
///

/// ErrorRequester::PrintException
// The following method will be called to print out the exception.
void ErrorRequester::PrintException(const char *fmt,...)
{
  char *newbuf;
  char buffer[128];
  int chrs;
  size_t oldlen = 0;
  va_list args;
  va_start(args,fmt);
  //
  // First check how much buffer we need with vsnprintf
  chrs = vsnprintf(buffer,127,fmt,args);
  if (chrs == -1) {
    // Bug workaround. Truncate it.
    chrs = 127;
  }
  va_end(args);
  va_start(args,fmt);
  // Check whether the current node to log with already has a buffer. If so,
  // build a new one and attach the result of the current printing to form
  // a complete output.
  if (NewLog->Error)
    oldlen = strlen(NewLog->Error);
  newbuf   = new char[oldlen + 1 + chrs];
  //
  // Copy contents over. This operation will never throw.
  if (NewLog->Error)
    strcpy(newbuf,NewLog->Error);
  //
  // Copy new data to the end of old data with the full length.
  vsnprintf(newbuf + oldlen,chrs,fmt,args);
  //
  // And exchange the buffer contents.
  delete[] NewLog->Error;
  NewLog->Error = newbuf;
  //
  // done with it.
  va_end(args);
}
///

/// ErrorRequester::Request
// Run the requester with the given exception. If this returns zero, then
// the requester could not be build for whatever reason and the message
// must be made available in another way, e.g. by re-throwing it.
int ErrorRequester::Request(const AtariException &except)
{  
  int result = ERQ_Menu;
  // Build up a new error message and attach it at
  // the end of the log.
  ErrorLog.AddTail(NewLog = new struct ErrorTxt);
  //
  // Run the exception printer on it to turn this into a text.
  except.PrintException(*this);
  //
  // Avoid calling this twice
  if (!active) {
    active = true;
    // Go on and request now the requester.... 
    result = Requester::Request();
    active = false;
  } else {
    if (machine->hasGUI()) {
      try {
	class AtariDisplay *display = machine->Display();
	if (display) {
	  display->ActiveBuffer();
	  // If we go here, the requester came in recursively.
	  // Test wether the display is the cause and if so,
	  // do not attempt to re-open the display.
	  //
	  // If the code comes here, everything worked.
	  // and we can attempt to open the display.
	} else {
	  return ERQ_Cancel;
	}
      } catch(AtariException &aex) {
	return ERQ_Cancel;
      }
    }
  }
  return result;
}
///
