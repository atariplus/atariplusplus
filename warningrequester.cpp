/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: warningrequester.cpp,v 1.12 2020/04/05 13:32:10 thor Exp $
 **
 ** In this module: A requester class that prints and logs warnings.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "warningrequester.hpp"
#include "exceptions.hpp"
#include "textgadget.hpp"
#include "buttongadget.hpp"
#include "listbrowsergadget.hpp"
#include "new.hpp"
#include "stdio.hpp"
#include "machine.hpp"
#include <ctype.h>
///

/// WarningRequester::WarningRequester
WarningRequester::WarningRequester(class Machine *mach)
  : Requester(mach), Headline(NULL), OKGadget(NULL), MenuGadget(NULL)
{
}
///

/// WarningRequester::~WarningRequester
WarningRequester::~WarningRequester(void)
{
  struct TextNode *warn;
  // 
  // Dispose all warnings here.
  while ((warn = WarnLog.RemHead())) {
    delete warn;
  }
  //
  // The gadgets are disposed by the superclass here.
}
///

/// WarningRequester::Warning::Warning
// Build up a new warning message.
WarningRequester::Warning::Warning(const char *msg)
  : Warn(NULL)
{
  char *newmsg;
  //
  //
  // Allocate a new string and copy us in. If this fails, then
  // this object will get destroyed.
  newmsg = new char[strlen(msg) + 1];
  strcpy(newmsg,msg);
  Warn   = newmsg;
}
///

/// WarningRequester::Warning::~Warning
// Shut down a warning requester message.
WarningRequester::Warning::~Warning(void)
{
  delete[] TOCHAR(Warn);
}
///

/// WarningRequester::BuildGadgets
// Build up the gadgets for this requester. We currently
// only present a title, the contents, and an "OK" gadget.
void WarningRequester::BuildGadgets(List<Gadget> &glist,class RenderPort *rport)
{
  class ListBrowserGadget *lb;
  LONG w,h;
  // Just some consistency checks.
#if CHECK_LEVEL > 0
  if (Headline || OKGadget || MenuGadget)
    Throw(ObjectExists,"WarningRequester::BuildGadgets",
	  "requester is already build up");
#endif
  // Get dimension of the render port to adjust the size of the gadgets.
  w = rport->WidthOf();
  h = rport->HeightOf();
  //
  Headline = new TextGadget(glist,rport,0,0,w,12,"Atari++ Warning");
  //
  lb       = new ListBrowserGadget(glist,rport,4,18,w - 8,h - 18 - 18,&WarnLog);
  // Scroll the scroller to the bottommost position to ensure that we see the
  // last message which is at the bottom.
  lb->ScrollTo(0xffff);
  //
  // Add up an OK gadget to the end.
  OKGadget   = new ButtonGadget(glist,rport,w >> 1,h - 12,w - (w >> 1),12,"Dismiss");
  MenuGadget = new ButtonGadget(glist,rport,0     ,h - 12,w >> 1      ,12,"Enter Menu");
}
///

/// WarningRequester::HandleEvent
// Event handling callback.
int WarningRequester::HandleEvent(struct Event &event)
{
  if (event.Type == Event::GadgetUp) {
    if (event.Object == OKGadget)
      return WRQ_Retry;
    if (event.Object == MenuGadget)
      return WRQ_Menu;
  }
  return RQ_Nothing;
}
///

/// WarningRequester::CleanupGadgets
// Gadget cleaner routine. Note that the
// gadgets itself are successfully destroyed by
// the superclass already.
void WarningRequester::CleanupGadgets(void)
{
  // Just clean the pointers to indicate that we
  // are ready to take new gadgets here.
  Headline   = NULL;
  OKGadget   = NULL;
  MenuGadget = NULL;
}
///

/// WarningRequester::Request
// Build up a warning requester with the given warning message.
int WarningRequester::Request(const char *msg)
{
  int result;
  // Build up a new warning message and attach it at
  // the end of the log.
  WarnLog.AddTail(new struct Warning(msg));
  //
  if (isHeadLess()) {
    const struct TextNode *warn = WarnLog.First();
    char in;
    SwitchGUI(false);
    //
#if MUST_OPEN_CONSOLE
    OpenConsole();
#endif
    printf("Atari++ warning history log:\n\n");
    //
    while(warn) {
      printf("%s\n",warn->Text());
      warn = warn->NextOf();
    }
    //
    //
    printf("\t(A): Continue Execution\n"
	   "\t(B): Cold Start (Reboot)\n"
	   "\t(C): Warm Start (Reset)\n"
	   "\t(D): Enter Monitor\n"
	   "\t(E): Quit\n\n");
    //
    do {
      printf("\nYour choice: ");
      fflush(stdout);
      //
      scanf("%c",&in);
      in = toupper(in);
    } while(in < 'A' || in > 'E');
    SwitchGUI(true);
    //
    switch(in) {
    case 'A':
      return 0;
    case 'B':
      throw AsyncEvent(AsyncEvent::Ev_ColdStart);
    case 'C':
      throw AsyncEvent(AsyncEvent::Ev_WarmStart);
    case 'D':
      MachineOf()->LaunchMonitor() = true;
      return 0;
    case 'E':
      throw AsyncEvent(AsyncEvent::Ev_Exit);
    }
  }
  //
  // Go on and request now the requester....
  result = Requester::Request();
  if (result == WRQ_Menu)
    throw AsyncEvent(AsyncEvent::Ev_EnterMenu);
  
  return result;
}
///

