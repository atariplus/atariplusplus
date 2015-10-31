/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: choicerequester.cpp,v 1.11 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: A requester that allows the user to pick multiple choices
 **********************************************************************************/

/// Includes
#include "event.hpp"
#include "renderport.hpp"
#include "buttongadget.hpp"
#include "textgadget.hpp"
#include "listbrowsergadget.hpp"
#include "choicerequester.hpp"
#include "exceptions.hpp"
#include "machine.hpp"
#include "new.hpp"
#include <stdarg.h>
#include <ctype.h>
///

/// Forwards
class ButtonGadget;
class TextGadget;
class ListBrowserGadget;
///

/// ChoiceRequester::ChoiceRequester
ChoiceRequester::ChoiceRequester(class Machine *mach)
  : Requester(mach)
{
}
///

/// ChoiceRequester::~ChoiceRequester
ChoiceRequester::~ChoiceRequester(void)
{
  struct GadgetNode *node;
  //
  // No need to clean up the gadgets, but clean up at least
  // the gadget nodes. The text isn't ours.
  while((node = GadgetList.RemHead())) {
    delete node;
  }
}
///

/// ChoiceRequester::GadgetNode::GadgetNode
// Build up a gadget node from a text. This just
// enters the text into the node.
ChoiceRequester::GadgetNode::GadgetNode(const char *text,int id)
  : Button(NULL), GadgetText(text), Id(id)
{ }
///

/// ChoiceRequester::GadgetNode::BuildUpGadget
// Build-up a gadget from the text.
void ChoiceRequester::GadgetNode::BuildUpGadget(List<Gadget> &glist,class RenderPort *rp,LONG le,LONG te,LONG w,LONG h)
{
  Button = new ButtonGadget(glist,rp,le,te,w,h,GadgetText);
}
///

/// ChoiceRequester::BuildGadgets
// Build up the gadgets for this requester.
void ChoiceRequester::BuildGadgets(List<Gadget> &glist,class RenderPort *rport)
{
  int cnt,gadsperline;
  int maxwidth;
  LONG width,height;
  LONG le,te;
  struct GadgetNode *gn;
  struct TextContents body(BodyText);
  // The list containing the text, as required for the list browser.
  List<struct TextNode> bodylist;
  //
  //
  // Count the number of selections we have and compute the room we have
  // per gadget; we need to compute how LONG a gadget can grow at most.
  maxwidth = 0;
  cnt      = 0;
  gn       = GadgetList.First();
  while(gn) {
    size_t len = strlen(gn->GadgetText);
    if (len > size_t(maxwidth)) {
      maxwidth = int(len);
    }
    cnt++;
    gn = gn->NextOf();
  }
  //
  // Compute the room required for the gadget.
  maxwidth    = (maxwidth << 3) + 4;
  // From that, we can deduce the number of gadgets
  // per line: Requires the renderport width.
  width       = rport->WidthOf();
  gadsperline = width / maxwidth;
  // If there is more room on this line than there
  // are gadgets, then reduce to the number of gadgets.
  if (gadsperline > cnt)
    gadsperline = cnt;
#if CHECK_LEVEL > 0
  if (gadsperline < 1) {
    // Oh well, then the gadget bodies are too LONG.
    Throw(OutOfRange,"ChoiceRequester::BuildGadgets","gadget texts are too LONG");
  }
#endif
  // Compute now the usable width : Possibly enlarge gadgets somewhat
  maxwidth    = width / gadsperline;
  // Now compute the height of the listbrowser gadget as the remaining height.
  height      = rport->HeightOf() - gadsperline * 12 - 8;
#if CHECK_LEVEL > 0
  if (height <= 12) {
    // No room left for the body text textbrowser gadget.
    Throw(OutOfRange,"ChoiceRequester::BuildGadgets","too many selections passed in");
  }
#endif
  //
  // Now build-up the contents for the listbrowser.
  bodylist.AddHead(&body);
  //
  // Now for the listbrowser.
  BodyGadget = new ListBrowserGadget(glist,rport,4,4,width-8,height-4,&bodylist);
  //
  // Now for the individual gadgets. Start at the left/top edge.
  le = 0,te = height+8;
  gn = GadgetList.First();
  while(gn) {
    gn->BuildUpGadget(glist,rport,le,te,maxwidth,12);
    le += LONG(maxwidth);
    if (le >= width) {
      le  = 0;
      te += 12;
    }
    gn = gn->NextOf();
  }
}
///

/// ChoiceRequester::CleanupGadgets
// Remove the gadgets from the requester. This
// need not to delete the gadget as this is the matter of
// the main requester class.
void ChoiceRequester::CleanupGadgets(void)
{ 
  struct GadgetNode *node;
  //
  // Remove all the gadget nodes while we are here.
  // The destructor will try again, but we'll do
  // this here anyhow.
  while((node = GadgetList.RemHead())) {
    delete node;
  }
  BodyGadget = NULL;
}
///

/// ChoiceRequester::HandleEvent
// Check whether any of our gadgets are hit. If so,
// return a high-level event to abort the requestor.
int ChoiceRequester::HandleEvent(struct Event &event)
{
  struct GadgetNode *gn;
  //
  if (event.Type == Event::GadgetUp) {
    // Iterate over all gadgets, check what has been hit
    // and generate the event then accordingly.
    gn = GadgetList.First();
    while(gn) {
      if (gn->Button == event.Object) {
	// Hit a gadget. Deliver the event Id.
	return gn->Id;
      }
      gn = gn->NextOf();
    }
  }
  //
  // Nothing happened.
  return RQ_Nothing;
}
///

/// ChoiceRequester::Request
// Build up a choice requester. This requires a body text, and a list of
// choices terminated by a NULL to pick from.
int ChoiceRequester::Request(const char *body,...)
{
  const char *text;
  va_list args;
  int id;
  va_start(args,body);
  //
  if (!isHeadLess()) {
    // First install the gadget body now.
    BodyText = body;
    // Start the next available Id that is
    // not used up by the requester here.
    id       = int(RQ_Abort);
    do {
      //
      // Get the next available possible selection.
      text = va_arg(args,const char *);
      if (text) {
	// Ok, this is a valid entry. Build a gadget text for it.
	GadgetList.AddTail(new struct GadgetNode(text,id));
	id++;
      }
    } while(text);
    //
    // And now check whether we can use the requester: Call the
    // requester main management.
    id = Requester::Request();
  } else {
    int ch    = 'A';
    int count = 0;
    char in;
    //
    SwitchGUI(false);
    printf("\n%s\n",body);
    //
    do {
      text = va_arg(args,const char *);
      if (text) {
	count++;
      }
    } while(text);
    //
    va_end(args);
    va_start(args,body);
    //
    do {
      text = va_arg(args,const char *);
      if (text) {
	if (count == 2) {
	  if (ch == 'A') {
	    printf("\t(N): %s\n",text);
	  } else if (ch == 'B') {
	    printf("\t(Y): %s\n",text);
	  }
	} else {
	  printf("\t(%c): %s\n",ch,text);
	}
	ch++;
      }
    } while(text);
    //
    do {
      printf("\nYour choice: ");
      fflush(stdout);
      //
      in = 0;
      scanf("%c",&in);
      in = toupper(in);
      //
      if (count == 2) {
	switch(in) {
	case 'Y':
	  in = 'B';
	  break;
	case 'N':
	case '\n':
	  in = 'A';
	  break;
	}
      }
    } while(in < 'A' || in >= ch);
    id = in - 'A' + int(RQ_Abort);
    //
    SwitchGUI(true);
  }
  //
  // That's it for the arguments.
  va_end(args);
  // If the main requester was aborted due to lack of resources
  // then return the default, namely the leftmost requester.
  // Otherwise, return the requester number. Note that the
  // gadgets are numbered from RQ_Abort and up.
  if (id >= int(RQ_Abort)) {
    return id - int(RQ_Abort);
  }
  return 0;
}
///

