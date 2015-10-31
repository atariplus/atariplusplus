/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filebuttongadget.hpp,v 1.4 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of a graphical button gadget 
 ** that brings up a file requester
 **********************************************************************************/

#ifndef FILEBUTTONGADGET_HPP
#define FILEBUTTONGADGET_HPP

/// Includes
#include "gadget.hpp"
///

/// FileButtonGadget
// A "browse file list" gadget that is to be used for bringing up a requester
// for the "file entry" gadget.
class FileButtonGadget : public Gadget {
  //
  // Set if the image is rendered as active.
  bool HitImage;
  //
public:
  FileButtonGadget(List<Gadget> &gadgetlist,
		   class RenderPort *rp,LONG le,LONG te,LONG w, LONG h);
  virtual ~FileButtonGadget(void);
  //
  // Perofrm action if the gagdet was hit, update the event to a higher-level event.
  virtual bool HitTest(struct Event &ev);
  //
  // Re-render the gadget
  virtual void Refresh(void);
};
///

///
#endif
