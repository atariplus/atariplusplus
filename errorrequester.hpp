/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: errorrequester.hpp,v 1.7 2014/06/01 20:07:53 thor Exp $
 **
 ** In this module: A requester class that prints and logs errors.
 **********************************************************************************/

#ifndef ERRORREQUESTER_HPP
#define ERRORREQUESTER_HPP

/// Includes
#include "list.hpp"
#include "requester.hpp"
#include "event.hpp"
#include "listbrowsergadget.hpp"
#include "exceptions.hpp"
///

/// Forwards
class ButtonGadget;
class TextGadget;
class Machine;
///

/// Class ErrorRequester
// This requester prints and logs errrors, i.e. conditions generated
// by exceptions.
class ErrorRequester : public Requester, private ExceptionPrinter {
  //
public:
  enum ErrorAction {
    ERQ_Nothing   = 0,                 // requester build-up failed
    ERQ_Cancel    = RQ_Abort,          // terminate the program
    ERQ_Monitor,                       // run the monitor
    ERQ_Menu,                          // enter the menu
    ERQ_Retry                          // retry the run
  };
private:
  // The following structure keeps recent warnings
  // as a text list. We must use the TextNode as base
  // class to use the list browser requester.
  struct ErrorTxt : public TextNode {
    //
    // Pointer to the text forming the error.
    char *Error;
    //
    ErrorTxt(void);
    virtual ~ErrorTxt(void);
    //
    // Get the text node.
    virtual const char *Text(void) const
    {
      return Error;
    }
  }                    *NewLog;  // The error log we are currently setting up
  //
  List<struct TextNode> ErrorLog;
  //
  // A couple of gadgets in here. (More to come)
  //
  class TextGadget     *Headline;
  class ButtonGadget   *CancelGadget;
  class ButtonGadget   *MonitorGadget;
  class ButtonGadget   *MenuGadget;
  class ButtonGadget   *OKGadget;
  //
  // The machine pointer.
  class Machine        *machine;
  //
  // Nest-counter. Avoids double errors.
  bool                  active;
  //
  // Gadget build up and destroy routines.
  virtual void BuildGadgets(List<Gadget> &glist,class RenderPort *rport);
  virtual void CleanupGadgets(void);
  //
  // Event handling callback.
  virtual int HandleEvent(struct Event &event);
  //  
  // The following method will be called to print out the exception.
  virtual void PrintException(const char *fmt,...) PRINTF_STYLE;
  //
public:
  // Form an error requester. Requires only the machine class.
  ErrorRequester(class Machine *mach);
  ~ErrorRequester(void);
  //
  // Run the requester with the given exception. If this returns zero, then
  // the requester could not be build for whatever reason and the message
  // must be made available in another way.
  int Request(const class AtariException &exc);
};
///

///
#endif
